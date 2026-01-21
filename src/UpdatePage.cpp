/*
 * UpdatePage.cpp - Firmware update page implementation
 */

#include "UpdatePage.h"
#include "Version.h"

String getUpdatePage() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ControlZinvoltP1 - Firmware Update</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        .header {
            text-align: center;
            color: white;
            margin-bottom: 30px;
        }
        .header h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
        }
        .header p {
            font-size: 1.1em;
            opacity: 0.9;
        }
        .page-links {
            margin-top: 16px;
            display: flex;
            justify-content: center;
            gap: 12px;
        }
        .page-link {
            padding: 10px 18px;
            border-radius: 999px;
            border: 1px solid rgba(255, 255, 255, 0.5);
            color: white;
            font-weight: 600;
            text-decoration: none;
            transition: background 0.3s, color 0.3s;
        }
        .page-link.active {
            background: white;
            color: #3e2c7f;
            border-color: white;
            cursor: default;
        }
        .page-link:hover {
            background: rgba(255, 255, 255, 0.2);
        }
        .card {
            background: white;
            border-radius: 12px;
            padding: 25px;
            margin-bottom: 20px;
            box-shadow: 0 8px 16px rgba(0, 0, 0, 0.1);
        }
        .card h2 {
            color: #667eea;
            margin-bottom: 20px;
            font-size: 1.5em;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        .upload-box {
            border: 3px dashed #667eea;
            border-radius: 12px;
            padding: 40px;
            text-align: center;
            background: #f8f9fa;
            cursor: pointer;
            transition: all 0.3s;
            margin-bottom: 20px;
        }
        .upload-box:hover {
            background: #e9ecef;
            border-color: #764ba2;
        }
        .upload-box.dragover {
            background: #e7e9fd;
            border-color: #764ba2;
            transform: scale(1.02);
        }
        .upload-icon {
            font-size: 3em;
            margin-bottom: 10px;
        }
        .file-info {
            margin-top: 20px;
            padding: 15px;
            background: #d4edda;
            border-radius: 8px;
            border-left: 4px solid #28a745;
            display: none;
        }
        .file-error {
            margin-top: 20px;
            padding: 15px;
            background: #f8d7da;
            border-radius: 8px;
            border-left: 4px solid #dc3545;
            display: none;
        }
        .upload-btn {
            display: inline-block;
            padding: 12px 30px;
            background: #667eea;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 1.1em;
            font-weight: bold;
            cursor: pointer;
            margin-top: 20px;
            transition: background 0.3s;
            display: none;
        }
        .upload-btn:hover {
            background: #764ba2;
        }
        .upload-btn:disabled {
            background: #ccc;
            cursor: not-allowed;
        }
        .progress-container {
            margin-top: 20px;
            display: none;
        }
        .progress-bar {
            width: 100%;
            height: 30px;
            background: #e9ecef;
            border-radius: 15px;
            overflow: hidden;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
            width: 0%;
            transition: width 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
        }
        .github-link {
            display: inline-block;
            padding: 12px 24px;
            background: #24292e;
            color: white;
            border-radius: 8px;
            text-decoration: none;
            font-weight: 600;
            margin-top: 10px;
            transition: background 0.3s;
        }
        .github-link:hover {
            background: #0366d6;
        }
        .version-info {
            background: #fff3cd;
            border-left: 4px solid #ffc107;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
        }
        .warning-box {
            background: #f8d7da;
            border-left: 4px solid #dc3545;
            padding: 15px;
            border-radius: 8px;
            margin-top: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>⚡ ControlZinvoltP1</h1>
            <p>ESP32-S3 P1 Controller for Zinvolt VT1000</p>
            <div class="page-links">
                <a class="page-link" href="/">Actuals</a>
                <a class="page-link" href="/settings">Settings</a>
                <span class="page-link active">Update</span>
            </div>
        </div>

        <div class="card">
            <h2>🔄 Firmware Update</h2>
            
            <div class="version-info">
                <strong>Current Version:</strong> )rawliteral";
  html += getFirmwareVersion();
  html += R"rawliteral(<br>
                <strong>Device:</strong> ESP32-S3-Mini-1<br>
                <strong>Free Heap:</strong> <span id="freeHeap">-</span> KB
            </div>

            <div class="warning-box">
                <strong>⚠️ Important:</strong> Only upload firmware files (firmware.bin) built for ESP32-S3. 
                Uploading incorrect files may brick your device. The update process takes 1-2 minutes.
            </div>

            <h3 style="margin-top: 30px;">Option 1: Download from GitHub</h3>
            <p>Download the latest pre-built firmware from the GitHub releases page:</p>
            <a href="https://github.com/yourusername/ControlZinvoltP1/releases" target="_blank" class="github-link">
                📦 Download Latest Release from GitHub
            </a>

            <h3 style="margin-top: 30px;">Option 2: Upload Custom Firmware</h3>
            <div class="upload-box" id="uploadBox">
                <div class="upload-icon">📁</div>
                <h3>Drag & Drop firmware.bin here</h3>
                <p>or click to select file</p>
                <input type="file" id="fileInput" accept=".bin" style="display: none;">
            </div>

            <div class="file-info" id="fileInfo">
                <strong>Selected File:</strong> <span id="fileName"></span><br>
                <strong>Size:</strong> <span id="fileSize"></span> KB<br>
                <span id="validationMsg"></span>
            </div>

            <div class="file-error" id="fileError"></div>

            <button class="upload-btn" id="uploadBtn">Upload Firmware</button>

            <div class="progress-container" id="progressContainer">
                <div class="progress-bar">
                    <div class="progress-fill" id="progressFill">0%</div>
                </div>
                <p id="statusMsg" style="text-align: center; margin-top: 10px; font-weight: bold;"></p>
            </div>
        </div>
    </div>

    <script>
        let selectedFile = null;

        // Update free heap
        function updateHeap() {
            fetch('/api/status')
                .then(r => r.json())
                .then(data => {
                    if (data.freeHeap) {
                        document.getElementById('freeHeap').textContent = Math.round(data.freeHeap / 1024);
                    }
                })
                .catch(e => console.error('Failed to fetch heap:', e));
        }
        updateHeap();

        const uploadBox = document.getElementById('uploadBox');
        const fileInput = document.getElementById('fileInput');
        const fileInfo = document.getElementById('fileInfo');
        const fileError = document.getElementById('fileError');
        const uploadBtn = document.getElementById('uploadBtn');
        const progressContainer = document.getElementById('progressContainer');
        const progressFill = document.getElementById('progressFill');
        const statusMsg = document.getElementById('statusMsg');

        // Click to select file
        uploadBox.addEventListener('click', () => fileInput.click());

        // File selection
        fileInput.addEventListener('change', (e) => {
            handleFile(e.target.files[0]);
        });

        // Drag & drop
        uploadBox.addEventListener('dragover', (e) => {
            e.preventDefault();
            uploadBox.classList.add('dragover');
        });

        uploadBox.addEventListener('dragleave', () => {
            uploadBox.classList.remove('dragover');
        });

        uploadBox.addEventListener('drop', (e) => {
            e.preventDefault();
            uploadBox.classList.remove('dragover');
            handleFile(e.dataTransfer.files[0]);
        });

        function handleFile(file) {
            fileError.style.display = 'none';
            fileInfo.style.display = 'none';
            uploadBtn.style.display = 'none';

            if (!file) return;

            // Validate file extension
            if (!file.name.endsWith('.bin')) {
                showError('Invalid file type. Please select a .bin firmware file.');
                return;
            }

            // Validate file size (ESP32-S3 firmware should be < 2MB typically)
            const maxSize = 3 * 1024 * 1024; // 3MB max
            const minSize = 100 * 1024; // 100KB min
            if (file.size > maxSize) {
                showError('File too large. Maximum size is 3MB.');
                return;
            }
            if (file.size < minSize) {
                showError('File too small. Minimum size is 100KB. This may not be a valid firmware file.');
                return;
            }

            // Validate ESP32 magic bytes (first byte should be 0xE9 for ESP32 app)
            const reader = new FileReader();
            reader.onload = (e) => {
                const arr = new Uint8Array(e.target.result);
                if (arr[0] !== 0xE9) {
                    showError('Invalid ESP32 firmware file. Missing magic byte (0xE9).');
                    return;
                }

                // File is valid
                selectedFile = file;
                document.getElementById('fileName').textContent = file.name;
                document.getElementById('fileSize').textContent = Math.round(file.size / 1024);
                document.getElementById('validationMsg').innerHTML = '<span style="color: #28a745;">✓ Valid ESP32 firmware detected</span>';
                fileInfo.style.display = 'block';
                uploadBtn.style.display = 'inline-block';
            };
            reader.readAsArrayBuffer(file.slice(0, 4)); // Read first 4 bytes
        }

        function showError(message) {
            fileError.textContent = message;
            fileError.style.display = 'block';
        }

        // Upload firmware
        uploadBtn.addEventListener('click', async () => {
            if (!selectedFile) return;

            uploadBtn.disabled = true;
            fileInfo.style.display = 'none';
            progressContainer.style.display = 'block';
            statusMsg.textContent = 'Uploading firmware...';

            try {
                // Use AsyncElegantOTA endpoint
                const xhr = new XMLHttpRequest();
                
                xhr.upload.addEventListener('progress', (e) => {
                    if (e.lengthComputable) {
                        const percent = Math.round((e.loaded / e.total) * 100);
                        progressFill.style.width = percent + '%';
                        progressFill.textContent = percent + '%';
                    }
                });

                xhr.addEventListener('load', () => {
                    if (xhr.status === 200) {
                        progressFill.style.width = '100%';
                        progressFill.textContent = '100%';
                        statusMsg.textContent = '✓ Upload complete! Device is rebooting...';
                        statusMsg.style.color = '#28a745';
                        setTimeout(() => {
                            alert('Firmware updated successfully! The device is rebooting. Please wait 10 seconds and refresh this page.');
                            location.reload();
                        }, 3000);
                    } else {
                        statusMsg.textContent = '✗ Upload failed: ' + xhr.statusText;
                        statusMsg.style.color = '#dc3545';
                        uploadBtn.disabled = false;
                    }
                });

                xhr.addEventListener('error', () => {
                    statusMsg.textContent = '✗ Upload error. Please try again.';
                    statusMsg.style.color = '#dc3545';
                    uploadBtn.disabled = false;
                });

                const formData = new FormData();
                formData.append('update', selectedFile);

                xhr.open('POST', '/update');
                xhr.send(formData);
            } catch (error) {
                statusMsg.textContent = '✗ Error: ' + error.message;
                statusMsg.style.color = '#dc3545';
                uploadBtn.disabled = false;
            }
        });
    </script>
</body>
</html>
)rawliteral";

  return html;
}
