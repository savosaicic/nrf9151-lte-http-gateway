# nrf9151-lte-http-gateway
A minimal end-to-end demo using an nRF9151 DK running Zephyr RTOS to send a button-press event over LTE-M via HTTP to a simple Flask server. This project is Intended as a learning starter.

## Architecture
**Data flow:**
1. Board boots → connects to LTE-M network
2. User presses Button 1
3. Firmware sends `POST /api/event` with a JSON payload
4. Flask server stores the event and responds with `201 Created`
5. Dashboard at `http://<server>:5000/` shows all events in real time
## Repository Structure

```
cellular-iot-gateway/
│
├── west.yml                   # West manifest — pins the nRF Connect SDK version
│
├── firmware/                  # Zephyr application
│   ├── CMakeLists.txt         # Build system entry point
│   ├── prj.conf               # Kconfig: enables modem, LTE, HTTP, GPIO…
│   └── src/
│       └── main.c             # Entrypoint
│
├── server/                    # Python Flask backend
│   ├── server.py              # API + web dashboard
│   ├── requirements.txt       # Python dependencies
│   └── templates/
│       └── dashboard.html     # Web UI showing received events
│
├── .gitignore
└── README.md
```

## Prerequisites
### Hardware
- A nRF9151dk board with a SIM card
### Software
- Zephyr SDK / toolchain (follow the [Getting Started](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) guide)


## Firmware Setup
### 1. Prepare the workspace and initialise this repo

```bash
mkdir nrf9151-ltem-http-gateway-ws && cd nrf9151-ltem-http-gateway-ws
west init -m https://github.com/savosaicic/nrf9151-lte-http-gateway --mr main
west update
pip install -r zephyr/scripts/requirements.txt
```

### 2. Configure the server hostname
Before building, update the `SERVER_HOSTNAME` value in the firmware to match your server address.

Example:
- `SERVER_HOSTNAME = "example.com"`
- or `SERVER_HOSTNAME = "203.0.113.10"`

### 3. Build & flash
```bash
cd nrf9151-lte-http-gateway
west build -b nrf9151dk/nrf9151/ns firmware/app
west flash
```

## Server Setup
If you want the board to reach your server, make sure port 5000 is reachable (firewall / port forwarding).
### 1. Install Python dependencies
```bash
cd server/
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```
### 2. Run the server
```bash
python server.py
```

You should see:
```
 * Running on http://0.0.0.0:5000
```
### 3. Open the dashboard
Navigate to `http://YOURIP:5000/` in your browser.
You will see a live table of all events received from the board.
