import json
import time
import webbrowser
from flask import Flask, jsonify, request
import requests
import sounddevice as sd
import numpy as np
import threading
from pynput import keyboard
from fingerprinting.communication import recognize_song_from_signature
from fingerprinting.algorithm import SignatureGenerator
from collections import deque
from concurrent.futures import ThreadPoolExecutor
import copy
import threading
from pydub import AudioSegment
from json import dumps
import os
import musicbrainzngs
import logging
import unidecode
import websocket
from youtube_search import YoutubeSearch

app = Flask(__name__)
BUFFER_DURATION = 10  # seconds
SAMPLERATE = 16000
BUFFER_SIZE = BUFFER_DURATION * SAMPLERATE
audio_buffer = deque(maxlen=BUFFER_SIZE)
KODI_IP = '192.168.1.212'
KODI_PORT = 8080 
KODI_WEBSOCKET_PORT = 9090
start_time = 0
executor = ThreadPoolExecutor(1)  # Create a thread pool with one worker thread
DEFAULT_SOUND_INPUT = "SPDIF (RME HDSPe RayDAT), MME"
#DEFAULT_SOUND_INPUT = "Microphone (2 - High Definition Audio Device)"
# "SPDIF (RME HDSPe RayDAT), MME"

logging.basicConfig(level=logging.DEBUG)

# Event object to signal the thread to stop
stop_event = threading.Event()
file_lock = threading.Lock()

key_to_check = 'r'

# Data structure to save tracklist information, keyed by discid
tracklist_dict = {}

# Writing to JSON with metadata
def db_persist(data_dict, filename='app.json'):
    with file_lock:
        with open(filename, 'w') as f:
            json.dump(data_dict, f)

# Reading from JSON with metadata
def db_read(filename='app.json'):
    with file_lock:
        try:
            with open(filename, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            return None         

# Function to update specific data by key
def db_update_by_key(key, new_value, filename='app.json'):
    with file_lock:
        # Read existing data
        try:
            with open(filename, 'r') as f:
                data = json.load(f)
        except FileNotFoundError:
            data = {}  

        # Update data
        data[key] = new_value

        # Write updated data back to file
        with open(filename, 'w') as f:
            json.dump(data, f)

# Function to update specific data by key
def db_update_by_key_if_not_exists(key, new_value, filename='app.json'):
    with file_lock:
        # Read existing data
        try:
            with open(filename, 'r') as f:
                data = json.load(f)
        except FileNotFoundError:
            data = {}  

        # Update data
        if key not in data:
            data[key] = new_value
            # Write updated data back to file
            with open(filename, 'w') as f:
                json.dump(data, f)            


# Function to read specific data by key
def db_read_by_key(key, filename='app.json'):
    with file_lock:
        try:
            with open(filename, 'r') as f:
                data = json.load(f)
            return data.get(key, None)
        except FileNotFoundError:
            return {}
  
@app.route('/buffer', methods=['POST'])
def post_buffer():
    global audio_buffer

    # Sending the audio buffer as binary data
    return requests.Response(audio_buffer, content_type='application/octet-stream')

def continuous_recording():
    global audio_buffer

    #print(sd.query_devices())

    with sd.InputStream(device=DEFAULT_SOUND_INPUT, samplerate=SAMPLERATE, channels=1, dtype='int16') as stream:
        while not stop_event.is_set():
                audio_chunk, overflowed = stream.read(SAMPLERATE)  # Read 1 second of audio
                audio_buffer.extend(audio_chunk)

def momentary_recording():
    global audio_buffer

    with sd.InputStream(device=DEFAULT_SOUND_INPUT, samplerate=SAMPLERATE, channels=1, dtype='int16') as stream:
        audio_chunk, overflowed = stream.read(SAMPLERATE)  # Read 1 second of audio
        audio_buffer.extend(audio_chunk)

def record_audio_from_mic(duration=5, samplerate=SAMPLERATE):
    """Record audio from the default microphone for a specified duration."""
    audio_data = sd.rec(int(samplerate * duration), samplerate=SAMPLERATE, channels=1, dtype='int16')
    sd.wait()  # Wait for the recording to finish
    return audio_data.flatten().tolist()

def on_press(key):

    try:
        if key.char == 'q':
            stop_event.set()
        if key.char == key_to_check:
            print("(disabled) R key pressed. Recognizing song...")
            #recognized_song = recognize_song_from_buffer(350, time.time())
            #print("Recognized song: {}".dumps(recognized_song, indent = 4, ensure_ascii = False))
    except AttributeError:
        pass

def convert_duration_to_seconds(duration):
    # Convert YouTube duration format (PT1M33S) to seconds
    hours = 0
    minutes = 0
    seconds = 0

    time_units = {
        'H': lambda value: 3600 * int(value),
        'M': lambda value: 60 * int(value),
        'S': lambda value: int(value)
    }

    start = 2  # Skip the 'PT' prefix
    value = ''

    for char in duration[start:]:
        if char in time_units:
            seconds += time_units[char](value)
            value = ''
        else:
            value += char

    return seconds    

# Function to send JSON-RPC requests to Kodi
def send_jsonrpc_request(method, params={}, id=1):
    url = "http://{}:{}/jsonrpc".format(KODI_IP, KODI_PORT)
    headers = {'content-type': 'application/json'}    

    # Prepare the payload to play the YouTube video
    payload = {
        "jsonrpc": "2.0",
        "method": method,
        "params": params,
        "id": id
    }

    response = requests.post(url, data=json.dumps(payload), headers=headers)
    #response = requests.post(url, json=payload)
    return response.json()

def on_message(ws, message):
    global start_time

    data = json.loads(message)

    if 'method' in data:
        print(data['method'])
        if data['method'] == 'Player.OnAVStart':
            print("!!!!! Playback started")
            # Get active players
            result = send_jsonrpc_request("Player.GetActivePlayers")
            print(unidecode.unidecode(json.dumps(result,ensure_ascii = False)))
            player_id = result['result'][0]['playerid']

            # Get current playback time
            result = send_jsonrpc_request("Player.GetProperties", {
                "playerid": player_id,
                "properties": ["time"]
            })
            print(unidecode.unidecode(json.dumps(result,ensure_ascii = False)))
            current_time = result['result']['time']
            start_time2 = time.time() - start_time
            #start_time2 -= 1
            if start_time2 < 0:
                start_time2 = 0
            print("start time1 {}".format(start_time))
            print("start time2 {}".format(start_time2))
            new_time = {
                "seconds": abs(current_time['seconds'] - int(start_time2)),
            }

            seconds = int(start_time2)  # Get the integer part (seconds)
            milliseconds = int((start_time2 - seconds) * 1000)  # Get the fractional part and convert to milliseconds

            total_milliseconds = current_time['milliseconds'] + milliseconds + 1300
            remaining_milliseconds = 0
            if total_milliseconds > 0:
                # Convert total milliseconds to seconds and remaining milliseconds
                seconds += total_milliseconds // 1000  # Integer division by 1000
                remaining_milliseconds = total_milliseconds % 1000  # Modulo 1000 to get remaining milliseconds


            new_time = {
                "time": {
                    "hours": current_time['hours'],
                    "minutes": current_time['minutes'],
                    "seconds": current_time['seconds'] + seconds,
                    "milliseconds": remaining_milliseconds
                }
            }

            # Seek to new time
            result = send_jsonrpc_request("Player.Seek", {
                "playerid": player_id,
                "value": new_time
            })    
            if "error" in result:
                # Seek to new time
                result = send_jsonrpc_request("Player.Seek", {
                    "playerid": player_id,
                    "value": new_time
                })    

            print(unidecode.unidecode(json.dumps(result,ensure_ascii = False)))


def play_youtube_video_on_kodi(kodi_url, kodi_port, youtube_video_id):
    url = "http://{}:{}/jsonrpc".format(kodi_url, kodi_port)
    headers = {'content-type': 'application/json'}
    
    # Prepare the payload to play the YouTube video
    payload = {
        "jsonrpc": "2.0",
        "method": "Player.Open",
        "params": {
            "item": {
                "file": "plugin://plugin.video.youtube/?action=play_video&videoid={}&start={}".format(youtube_video_id, start_time)
            }
        },
        "id": 1
    }
    
    # Send the request to Kodi
    response = requests.post(url, data=json.dumps(payload), headers=headers)
    
    # Check if the request was successful
    if response.status_code == 200:
        print("Successfully sent play command to Kodi.")
    else:
        print("Failed to send play command to Kodi. Status code: {}".format(response.status_code))

def time_to_seconds(time_str):
    minutes, seconds = map(int, time_str.split(":"))
    return minutes * 60 + seconds

def filter_youtube_response(track, search_response, duration, title_contains=""):
        print("Look up: {}".format(title_contains))
        closest_videos = []    
        for item in search_response:
            if title_contains and item.get('title') is not None and track.lower() not in item['title'].lower():
                continue
            video_id = item['id']
            video_duration_seconds = time_to_seconds(item['duration'])  # Format: MM:SS

            # Calculate the difference in duration
            print(video_duration_seconds - duration)
            duration_diff = abs(video_duration_seconds - duration)
            #print("{} {} ".format(video_id, duration_diff))
            if not title_contains:
                closest_videos.append((video_id, duration_diff))
            elif item.get('title') is not None and title_contains.lower() in item['title'].lower():
                print("optimizing for title {}".format(title_contains))
                closest_videos.append((video_id, duration_diff))

        return closest_videos    

def find_youtube_track(artist, track, current_time=0, duration=300, discid=None, trackNumber=0):
    global start_time

    search_query = '{} {} official video'.format(artist, track)
    print(discid)
    print(trackNumber)
    use_cache = True
    print("should query Youtube?")

    if discid is not None and 'videos' not in tracklist_dict[discid][trackNumber] or discid is not None and len(tracklist_dict[discid][trackNumber]['videos']) == 0:
        print("querying youtube")
        search_response = YoutubeSearch(search_query, max_results=5).to_dict()

        print(json.dumps(search_response, indent=4))

        #video_ids = [item['id'] for item in search_response['videos']]

        closest_videos = filter_youtube_response(track, search_response, duration, "video")
        if len(closest_videos) == 0:
            closest_videos = filter_youtube_response(track, search_response, duration, "M/V")
        if len(closest_videos) == 0:
            closest_videos = filter_youtube_response(track, search_response, duration)

        # Sort by duration_diff and take the top 3
        closest_videos = sorted(closest_videos, key=lambda x: x[1])[:3]
        #print(closest_videos)
        top_3_video_ids = [video[0] for video in closest_videos]

        if discid is not None:
            print("Saving Youtube search results to DB")
            tracklist_dict[discid][trackNumber]["videos"] = top_3_video_ids
            tracklist_dict[discid][trackNumber]["videos_offsets"] = [0, 0, 0]
            #tracklist_dict[discid][trackNumber]["videos_response"] = search_response
            db_update_by_key('tracklist', tracklist_dict)
    else:
        print("using cached results")
        top_3_video_ids = tracklist_dict[discid][trackNumber]["videos"]  

    top_3_video_offsets = tracklist_dict[discid][trackNumber].get("videos_offsets", [0, 0, 0])
    print(top_3_video_ids)
    #print(time.time())
    #print(current_time)
    for video_id in top_3_video_ids:
        # Open the video in the default web browser at the specified time
        if current_time == 0:
            print("!!!! current time is 0, resetting time from NOW")
        start_time = time.time() if current_time == 0 else current_time
        start_time -= top_3_video_offsets[0]
        print("!!! setting start time ")
        print(start_time)
        video_url_with_time = 'https://www.youtube.com/watch?v={}&t={}'.format(video_id, start_time)
        print("time offset {}".format(start_time))
        play_youtube_video_on_kodi(KODI_IP, KODI_PORT, video_id)
        #webbrowser.open(video_url_with_time)
        break
        

    return top_3_video_ids


def recognize_song_from_buffer(duration = 300, time_offset = 0):
    wait_time = 5
    time.sleep(wait_time)
    # Convert audio data to a fingerprint

    signature_generator = SignatureGenerator()
    audio_data = copy.copy(np.array(audio_buffer).flatten().tolist())
    signature_generator.feed_input(audio_data)

    # Prefer starting at the middle at the song, and with a
    # substantial bit of music to provide.
    
    signature_generator.MAX_TIME_SECONDS = wait_time

    results = '(Not enough data for recognition)'    

    while not stop_event.is_set():
        signature = signature_generator.get_next_signature()
        
        if not signature:
            print(dumps(results, indent = 4, ensure_ascii = False))
        
        results = recognize_song_from_signature(signature)     


        if results['matches']:
                    #print(dumps(results, indent = 4, ensure_ascii = False))
                    # Accessing track title and subtitle
                    track_title = results['track']['title']
                    track_artist = results['track']['subtitle']
                    find_youtube_track(track_artist, track_title, time_offset, duration)
                    break
                
        else:
            
            print(('[ Note: No matching songs for the first %g seconds, ' +
                'typing to recognize more input... ]\n') % (signature_generator.samples_processed / 16000))
            
            audio_data = copy.copy(np.array(audio_buffer).flatten().tolist())
            signature_generator.feed_input(audio_data)            
            # signature_generator.MAX_TIME_SECONDS = 12 # min(12, signature_generator.MAX_TIME_SECONDS + 3) # DEBUG


    return results

# Start WebSocket in separate function
def start_websocket():
    ws = websocket.WebSocketApp("ws://{}:{}/jsonrpc".format(KODI_IP, KODI_WEBSOCKET_PORT),
                            on_message=on_message)
    while not stop_event.is_set():
        ws.run_forever()
        if not stop_event.is_set():
            print("WebSocket disconnected. Reconnecting...")
    print("WebSocket thread stopping")

def print_menu(devices):
    print("Available sound devices:")
    for i, device in enumerate(devices):
        print("{}. {}".format(i+1, device['name']))
    print("0. Exit")

def choose_device(devices):
    while True:
        print_menu(devices)
        choice = input("Select a sound device: ")
        if choice.isdigit():
            choice = int(choice)
            if choice == 0:
                print("Exiting...")
                break
            elif 1 <= choice <= len(devices):
                selected_device = devices[choice - 1]
                print("You selected: {}".format(selected_device['name']))
                # Do something with the selected device
                break
            else:
                print("Invalid choice. Please try again.")
        else:
            print("Invalid input. Please enter a number.")


def print_colored_cd_art():
    art = """
    \033[94m
    .-~~~-.
  .-~~~-_  \         _ _
  \      ~\ \       / ~  N\
   \      _\|      |      |\|
    \    /  \      |   _  | \
     \   |   \     \  / \  \  \
      \  |    \     \/   |   \  \
       \  \     \    |  /|    \  \
        \  \     \   \/_/      \  \ 
         \  \      \_/         \__\
          \__\                    \__\
           \__\                    \__\
    \033[0m
    """
    print(art)

if __name__ == '__main__':
    print_colored_cd_art()
    devices = sd.query_devices()
    choose_device(devices)

    tracklist_dict = db_read_by_key('tracklist')

    # Initialize the keyboard listener
    listener = keyboard.Listener(on_press=on_press)
    listener.start()
    t1 = None
    t2 = None

    try:
        # Start the continuous recording thread
        t1 = threading.Thread(target=continuous_recording, daemon=False)
        t1.start()

        #t2 = threading.Thread(target=start_websocket, daemon=True)
        #t2.start()


        # Run the Flask server
        app.run(host='0.0.0.0', port=5123)

        
        t1.join()
        #t2.join()
        # Start the key monitoring thread
        #t2= threading.Thread(target=key_monitor, daemon=True).start()
    except KeyboardInterrupt:
        print("CTRL+C pressed. Stopping all threads and listeners.")
    except Exception as e:
        print("An exception occurred: %g. Stopping all threads and listeners.", e)
    finally:
        # Stop the listener
        listener.stop()
        stop_event.set()
        #t1.join()
        #t2.join()

        print("All threads and listeners stopped.")
    
