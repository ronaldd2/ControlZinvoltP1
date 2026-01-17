# 🚀 Quick Start Guide - ControlZinvoltP1

Get your ESP32-S3 P1 Controller up and running in minutes!

## 📋 What You Need

- ✅ ESP32-S3 development board
- ✅ Universal P1 Port Dongle
- ✅ USB cable
- ✅ Computer with VS Code
- ✅ Smart meter with P1 port

## ⚡ 5-Minute Setup

### Step 1: Install Software (5 minutes)

1. **Install VS Code**
   - Download from: https://code.visualstudio.com/
   - Install and launch

2. **Install PlatformIO Extension**
   - Open VS Code
   - Click Extensions icon (or press `Ctrl+Shift+X`)
   - Search for "PlatformIO IDE"
   - Click Install
   - Restart VS Code if prompted

### Step 2: Open Project (1 minute)

1. **Open Folder**
   - `File` → `Open Folder`
   - Select `ControlZinvoltP1` folder
   - Click "Yes, I trust the authors" if prompted

2. **Wait for PlatformIO**
   - PlatformIO will automatically install dependencies
   - Watch the progress in the bottom toolbar
   - Takes 2-3 minutes on first run

### Step 3: Build & Upload (3 minutes)

1. **Connect ESP32-S3**
   - Plug USB cable into ESP32-S3
   - Connect to computer
   - Windows should recognize it (driver auto-installs)

2. **Build the Firmware**
   - Click the ✓ checkmark icon in bottom toolbar
   - Or press `Ctrl+Alt+B`
   - Wait for "SUCCESS" message

3. **Upload to ESP32-S3**
   - Click the → arrow icon in bottom toolbar
   - Or press `Ctrl+Alt+U`
   - Wait for upload to complete

### Step 4: Configure WiFi (2 minutes)

1. **Look for WiFi Network**
   - ESP32-S3 creates: `ControlZinvoltP1-Setup`
   - Connect with phone or computer
   - Captive portal opens automatically

2. **Enter WiFi Credentials**
   - Select your WiFi network
   - Enter password
   - Click Save
   - Device restarts

3. **Find Device IP**
   - Open Serial Monitor in VS Code (`Ctrl+Alt+S`)
   - Look for: "IP Address: xxx.xxx.xxx.xxx"
   - Write it down!

### Step 5: Access Web Interface (1 minute)

1. **Open Browser**
   - Go to: `http://[YOUR_DEVICE_IP]`
   - Example: `http://192.168.1.100`

2. **You're Done!** 🎉
   - Dashboard should load
   - Data will appear when P1 telegrams are received

## 🔌 Hardware Connection

```
Smart Meter P1 Port
    ↓
Universal P1 Dongle
    ↓ (GPIO44 = RX)
ESP32-S3
    ↓ (GPIO43 = TX)
P1 Output (to Zinvolt)
```

## 🎛️ First Configuration

1. **Set Operation Mode**
   - Start with "Unmodified Forward"
   - Test that data is flowing

2. **Configure Phases**
   - Set Battery Phase (where battery is connected)
   - Set Modify Phase (usually same as battery phase)

3. **Test Force Modes**
   - Try "Force Charge" with 1000W
   - Monitor in web interface
   - Increase power as needed

## 📱 Quick Commands

### Via Web Interface
- Just click the buttons! 😊

### Via REST API
```bash
# Get status
curl http://192.168.1.100/api/status

# Force charge at 5000W
curl "http://192.168.1.100/api/mode?value=2"
curl "http://192.168.1.100/api/power?value=5000"

# Turn off
curl "http://192.168.1.100/api/mode?value=1"
```

## 🐛 Common Issues

### "Can't see WiFi network"
- ✅ Wait 30 seconds after upload
- ✅ Check ESP32 has power (LED should be on)
- ✅ Restart ESP32

### "Upload failed"
- ✅ Hold BOOT button while clicking upload
- ✅ Check USB cable (data cable, not charge-only)
- ✅ Try different USB port

### "No P1 data"
- ✅ Check connections to GPIO44/43
- ✅ Verify smart meter is sending data
- ✅ Check serial monitor for errors

### "Can't access web interface"
- ✅ Verify IP address in serial monitor
- ✅ Ensure on same WiFi network
- ✅ Try pinging the device: `ping 192.168.1.100`

## 📖 What's Next?

- Read [PROJECT_README.md](PROJECT_README.md) for full documentation
- Check [TECHNICAL_DOCS.md](TECHNICAL_DOCS.md) for advanced topics
- Customize pin configuration if needed
- Integrate with home automation

## 💡 Pro Tips

1. **Serial Monitor is Your Friend**
   - Press `Ctrl+Alt+S` to open
   - Shows debug info, errors, and IP address

2. **Start Conservative**
   - Begin with low force power values (1000-2000W)
   - Gradually increase while monitoring

3. **Bookmark the IP**
   - Save device IP in browser favorites
   - Or set static IP in router

4. **Test Before Production**
   - Test all modes thoroughly
   - Monitor for 24 hours before relying on it

## 🆘 Need Help?

1. Check serial monitor for errors
2. Read the detailed documentation
3. Open an issue on GitHub
4. Check connections and settings

---

**Time to first run:** ~15 minutes  
**Difficulty:** Easy 🟢

**Enjoy your smart P1 controller!** ⚡
