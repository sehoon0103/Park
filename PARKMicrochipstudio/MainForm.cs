using System;
using System.Drawing;
using System.IO.Ports;
using System.Windows.Forms;

namespace ParkingMonitor
{
    public partial class MainForm : Form
    {
        private const int SLOT_COUNT = 25;
        private const int BAUD_RATE = 9600;
        private const byte REG_PARKING_STATUS = 0x10;

        private SerialPort _port;
        private Panel[] _slots = new Panel[SLOT_COUNT];
        private Label[] _slotLabels = new Label[SLOT_COUNT];
        private bool[] _occupied = new bool[SLOT_COUNT];

        private System.Windows.Forms.Timer _pollTimer;
        private int _currentSlot = 0;

        private string _rxBuffer = "";

        public MainForm()
        {
            InitializeComponent();
            BuildUI();
        }

        // ── UI 구성 ──────────────────────────────────────────────

        private void BuildUI()
        {
            this.Text = "실시간 주차현황 모니터링";
            this.Size = new Size(820, 620);
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.BackColor = Color.FromArgb(30, 30, 30);

            // 상단 컨트롤 바
            var toolbar = new Panel { Dock = DockStyle.Top, Height = 50, BackColor = Color.FromArgb(45, 45, 45) };
            this.Controls.Add(toolbar);

            var lblPort = new Label { Text = "포트:", ForeColor = Color.White, Location = new Point(10, 15), AutoSize = true };
            toolbar.Controls.Add(lblPort);

            var cmbPort = new ComboBox { Location = new Point(45, 11), Width = 90, DropDownStyle = ComboBoxStyle.DropDownList };
            cmbPort.Items.AddRange(SerialPort.GetPortNames());
            if (cmbPort.Items.Count > 0) cmbPort.SelectedIndex = 0;
            toolbar.Controls.Add(cmbPort);

            var btnConnect = new Button { Text = "연결", Location = new Point(145, 10), Width = 60, Height = 28 };
            btnConnect.Click += (s, e) => ToggleConnection(cmbPort.SelectedItem?.ToString(), btnConnect);
            toolbar.Controls.Add(btnConnect);

            var lblStatus = new Label { Name = "lblStatus", Text = "● 연결 안됨", ForeColor = Color.OrangeRed, Location = new Point(220, 15), AutoSize = true };
            toolbar.Controls.Add(lblStatus);

            // 주차 슬롯 그리드 (5×5)
            var grid = new Panel { Location = new Point(20, 70), Size = new Size(760, 490), BackColor = Color.Transparent };
            this.Controls.Add(grid);

            const int cols = 5;
            const int cellW = 140, cellH = 90, padX = 10, padY = 10;

            for (int i = 0; i < SLOT_COUNT; i++)
            {
                int row = i / cols;
                int col = i % cols;

                var panel = new Panel
                {
                    Location = new Point(col * (cellW + padX), row * (cellH + padY)),
                    Size = new Size(cellW, cellH),
                    BackColor = Color.FromArgb(60, 180, 75),
                    Cursor = Cursors.Default
                };

                var label = new Label
                {
                    Text = $"{i + 1:D2}\n빈 자리",
                    ForeColor = Color.White,
                    Font = new Font("맑은 고딕", 11f, FontStyle.Bold),
                    TextAlign = ContentAlignment.MiddleCenter,
                    Dock = DockStyle.Fill
                };

                panel.Controls.Add(label);
                grid.Controls.Add(panel);

                _slots[i] = panel;
                _slotLabels[i] = label;
            }

            _pollTimer = new System.Windows.Forms.Timer { Interval = 100 };
            _pollTimer.Tick += PollTick;
        }

        // ── 시리얼 연결/해제 ──────────────────────────────────────

        private void ToggleConnection(string portName, Button btn)
        {
            if (_port != null && _port.IsOpen)
            {
                _pollTimer.Stop();
                _port.DataReceived -= Port_DataReceived;
                _port.Close();
                _port.Dispose();
                _port = null;
                btn.Text = "연결";
                UpdateStatus("● 연결 안됨", Color.OrangeRed);
                return;
            }

            if (string.IsNullOrEmpty(portName)) return;

            try
            {
                _port = new SerialPort(portName, BAUD_RATE, Parity.None, 8, StopBits.One)
                {
                    ReadTimeout = 300,
                    WriteTimeout = 300,
                    NewLine = "\n"
                };
                _port.DataReceived += Port_DataReceived;
                _port.Open();
                _currentSlot = 0;
                _rxBuffer = "";
                btn.Text = "해제";
                UpdateStatus($"● {portName} 연결됨", Color.LimeGreen);
                _pollTimer.Start();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"연결 실패: {ex.Message}", "오류", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        // ── 폴링: 슬레이브 순차 조회 ─────────────────────────────

        private void PollTick(object sender, EventArgs e)
        {
            if (_port == null || !_port.IsOpen) return;

            byte slaveId = (byte)(_currentSlot + 1);
            string frame = BuildReadFrame(slaveId, REG_PARKING_STATUS);
            try { _port.Write(frame); }
            catch { }
        }

        // ── 수신 처리 ────────────────────────────────────────────

        private void Port_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                _rxBuffer += _port.ReadExisting();
                int idx;
                while ((idx = _rxBuffer.IndexOf('\n')) >= 0)
                {
                    string line = _rxBuffer.Substring(0, idx + 1);
                    _rxBuffer = _rxBuffer.Substring(idx + 1);
                    ProcessFrame(line);
                }
            }
            catch { }
        }

        private void ProcessFrame(string frame)
        {
            // 슬레이브 응답 형식: :SSRRDDCC\n  (data 포함, R 커맨드 응답)
            if (frame.Length < 9 || frame[0] != ':') return;

            byte slaveId = HexToByte(frame, 1);
            char cmd = frame[3];
            if (cmd != 'R') return;

            byte address = HexToByte(frame, 4);
            if (address != REG_PARKING_STATUS) return;

            int dataLen = frame.Length == 11 ? 1 : 0;  // '\n' 포함 길이 11 = data 있음
            if (dataLen == 0) return;

            byte data = HexToByte(frame, 6);
            byte checksumRecv = HexToByte(frame, 8);

            // 체크섬 검증 (SS + R + RR + DD의 ASCII 합)
            byte checksumCalc = 0;
            for (int i = 1; i <= 7; i++) checksumCalc += (byte)frame[i];
            if (checksumCalc != checksumRecv) return;

            int slot = slaveId - 1;
            if (slot < 0 || slot >= SLOT_COUNT) return;

            bool isOccupied = data != 0;

            // UI 업데이트는 메인 스레드에서
            if (this.InvokeRequired)
                this.Invoke(new Action(() => UpdateSlot(slot, isOccupied)));
            else
                UpdateSlot(slot, isOccupied);

            // 다음 슬롯으로
            _currentSlot = (slot + 1) % SLOT_COUNT;
        }

        // ── 슬롯 UI 갱신 ─────────────────────────────────────────

        private void UpdateSlot(int slot, bool occupied)
        {
            if (_occupied[slot] == occupied) return;
            _occupied[slot] = occupied;

            _slots[slot].BackColor = occupied ? Color.FromArgb(200, 50, 50) : Color.FromArgb(60, 180, 75);
            _slotLabels[slot].Text = occupied ? $"{slot + 1:D2}\n✓ 주차중" : $"{slot + 1:D2}\n빈 자리";
        }

        // ── 프로토콜 헬퍼 ─────────────────────────────────────────

        // 마스터 READ 프레임: $SSRRRCC\n
        private static string BuildReadFrame(byte slaveId, byte address)
        {
            string body = $"{slaveId:X2}R{address:X2}";
            byte checksum = 0;
            foreach (char c in body) checksum += (byte)c;
            return $"${body}{checksum:X2}\n";
        }

        private static byte HexToByte(string s, int pos)
        {
            return (byte)(HexVal(s[pos]) << 4 | HexVal(s[pos + 1]));
        }

        private static int HexVal(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        }

        // ── 상태 표시 ─────────────────────────────────────────────

        private void UpdateStatus(string text, Color color)
        {
            var lbl = this.Controls.Find("lblStatus", true);
            if (lbl.Length > 0)
            {
                lbl[0].Text = text;
                lbl[0].ForeColor = color;
            }
        }
    }
}
