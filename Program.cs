using System;
using System.Drawing;
using System.Windows.Forms;
using System.Timers;

namespace MouseTracker
{
    class Program
    {
        static Label coordLabel;
        static System.Timers.Timer timer;

        [STAThread]
        static void Main()
        {
            // Windows Forms 시각적 스타일을 활성화 (문제 없이 실행되도록 보장)
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            // 윈도우(폼) 생성
            Form form = new Form();
            form.Text = "마우스 좌표 트래커";
            form.Size = new Size(400, 120);
            form.StartPosition = FormStartPosition.CenterScreen;
            form.FormBorderStyle = FormBorderStyle.FixedSingle;
            form.MaximizeBox = false;
            form.TopMost = true; // 항상 다른 창 위에 표시

            // 좌표를 표시할 레이블 생성
            coordLabel = new Label();
            coordLabel.Text = "X: 0  Y: 0";
            coordLabel.Font = new Font("맑은 고딕", 24, FontStyle.Bold);
            coordLabel.TextAlign = ContentAlignment.MiddleCenter;
            coordLabel.Dock = DockStyle.Fill;
            form.Controls.Add(coordLabel);

            // 0.05초(50ms)마다 마우스 좌표를 갱신하는 타이머
            timer = new System.Timers.Timer(50);
            timer.Elapsed += (sender, e) =>
            {
                Point pos = Cursor.Position;
                // 다른 스레드에서 UI를 안전하게 업데이트
                form.Invoke((MethodInvoker)(() =>
                {
                    coordLabel.Text = $"X: {pos.X}  Y: {pos.Y}";
                }));
            };
            timer.Start();

            // 프로그램 실행 (창 띄우기)
            Application.Run(form);
        }
    }
}
