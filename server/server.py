from flask import Flask, request, jsonify, render_template
from datetime import datetime

app = Flask(__name__)

# Store events in memory
events = []

@app.route('/')
def index():
    return render_template('dashboard.html', events=events)

@app.route('/api/events', methods=['POST'])
def receive_event():
    data = request.json
    
    if not data:
        return jsonify({"status": "error", "message": "No data provided"}), 400
    
    if 'event_type' not in data:
        return jsonify({"status": "error", "message": "event_type is required"}), 400
    
    data['timestamp'] = datetime.now().isoformat()
    
    events.append(data)
    
    if len(events) > 50:
        events.pop(0)
    
    print(f"[{data['event_type']}] Received: {data}")
    return jsonify({"status": "success"}), 201

@app.route('/api/events', methods=['GET'])
def get_events():
    return jsonify(events)

@app.route('/api/events/clear', methods=['POST'])
def clear_events():
    global events
    events = []
    return jsonify({"status": "success"}), 200

if __name__ == '__main__':
    print("Server starting...")
    app.run(host='0.0.0.0', port=5000, debug=True)
