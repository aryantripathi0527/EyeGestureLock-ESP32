# 👁️ Eye Gesture Authentication Lock (ESP32-S3)

## 📌 Overview
This project demonstrates a **biometric-inspired lock system** controlled entirely by **eye gestures** using an ESP32-S3 microcontroller.  
By detecting **LEFT** and **RIGHT** eye movements through EOG signals, the system authenticates a password sequence (e.g., `LEFT → LEFT → LEFT`) and unlocks access.

## 🎯 Features
- Guided calibration for reliable gesture detection  
- Real-time feedback via **Serial Monitor** and **Web UI**  
- Secure password sequence using eye gestures  
- Adaptive thresholds to handle noise and drift 

## 🛠️ Tech Stack
- **Hardware:** ESP32-S3, EOG electrodes, LED indicator  
- **Software:** Arduino IDE, C++ (ESP32 libraries), HTML/CSS/JS frontend  
- **Networking:** ESP32 WebServer for live status and gesture logs  

## ⚙️ How It Works
1. **Calibration:** User looks straight, left, and right to set baseline and thresholds.  
2. **Gesture Detection:** Eye movements are classified as LEFT (-1) or RIGHT (+1).  
3. **Password Check:** Sequence is compared against stored password.  
4. **Access Control:**  
   - ✅ Correct sequence → `ACCESS GRANTED` → LED ON → 🔓 lock icon  
   - ❌ Wrong sequence → `ACCESS DENIED` → LED OFF → 🔒 lock icon  

## 📹 Demo
Watch the full demo video here: *(attach your LinkedIn video link or upload to GitHub releases)*

## 💻 Code
The main Arduino sketch is included in this repository:  
- `sketch_dec24a.ino` → ESP32-S3 firmware  
- Web interface embedded in code (HTML/CSS/JS)  

## 🚀 Getting Started
1. Clone this repository:
   ```bash
   git clone https://github.com/aryantripathi0527/EyeGestureLock-ESP32.git
