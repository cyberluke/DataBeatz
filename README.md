
---

# DataBeatz: see CD on TV

This project captures audio input from the microphone on-demand (when the "R" key is pressed) and uses the SongRec library to recognize the song via the Shazam API.

## Prerequisites

- Windows XP or later
- Python 3.4.3 x86

## Installation

### 1. Download and Install Python 3.4.3 x86

Download Python 3.4.3 x86 from the official Python website's archive:

[Python 3.4.3 x86 Download Link](https://www.python.org/ftp/python/3.4.3/python-3.4.3.msi)

Run the installer and follow the on-screen instructions. Ensure you select the option to add Python to your system's PATH.

### 2. Install pip for Python 3.4

To install `pip` for Python 3.4 system-wide from the command line, follow these steps:

1. Download `get-pip.py`:

```bash
curl https://bootstrap.pypa.io/pip/3.4/get-pip.py -o get-pip.py
```

2. Run the `get-pip.py` script:

```bash
python get-pip.py
```

This will install `pip` for Python 3.4 system-wide.

### 3. Install Required Libraries

Navigate to the project directory and run:

```bash
pip install -r requirements.txt
```

This will install all the necessary libraries for the project.

## Running the Server

To run the Flask server:

```bash
python server.py
```

Once the server is running, press the "R" key to capture audio from the microphone and recognize the song using the Shazam API. The recognized song details will be printed to the console.

---

Certainly! Here's the additional section to add to the `README.md` file for downloading and installing `curl` on Windows XP:

---

### Installing curl on Windows XP

To download and use `curl` on Windows XP, follow these steps:

1. **Download the curl executable for Windows**:

   You can download a version of `curl` that's compatible with Windows XP from the [curl for Windows download page](https://curl.se/windows/). Look for the version that matches your system (32-bit or 64-bit) and is labeled as having SSL support.

2. **Install curl**:

   - Once downloaded, extract the ZIP file to a directory, e.g., `C:\curl`.
   - Add the directory to your system's PATH:
     1. Right-click on 'My Computer' and select 'Properties'.
     2. Navigate to the 'Advanced' tab.
     3. Click on 'Environment Variables'.
     4. In the 'System Variables' section, find the 'Path' variable, select it, and click 'Edit'.
     5. At the end of the 'Variable value' field, add `;C:\curl` (assuming you extracted `curl` to `C:\curl`).
     6. Click 'OK' to close each window.

3. **Verify the Installation**:

   Open a new command prompt and type:

   ```bash
   curl --version
   ```

   This should display the version of `curl` you installed, indicating that the installation was successful.
