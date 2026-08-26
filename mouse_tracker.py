import tkinter as tk
from tkinter import Label
import pyautogui
import threading
import time

def update_coordinates():
    """마우스 좌표를 실시간으로 업데이트하는 함수"""
    while True:
        x, y = pyautogui.position()
        coord_label.config(text=f"X: {x}  Y: {y}")
        time.sleep(0.05)  # 0.05초마다 업데이트 (성능 조절 가능)

# 메인 윈도우 생성
root = tk.Tk()
root.title("마우스 좌표 트래커")
root.geometry("250x100")
root.resizable(False, False)

# 좌표를 표시할 레이블 생성
coord_label = Label(root, text="X: 0  Y: 0", font=("Arial", 20))
coord_label.pack(expand=True)

# 스레드에서 좌표 업데이트 함수 실행 (GUI 블로킹 방지)
thread = threading.Thread(target=update_coordinates, daemon=True)
thread.start()

# GUI 이벤트 루프 실행
root.mainloop()
