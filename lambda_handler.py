import requests
from submission.secrets import spotify_token, spotify_user_id
from submission.refresh import Refresh
import json
import boto3

# Album to play the song from
ALBUM = "spotify:playlist:6k8AkuKcNRyEE2z52r8Vad"

ACCESS_KEY = 'YOUR_ACCESS_KEY'
SECRET_KEY = 'YOUR_SECRET_KEY'

client = boto3.client("iot-data", aws_access_key_id=ACCESS_KEY, aws_secret_access_key=SECRET_KEY,
                      region_name="us-west-2")
thing_name = 'sns_thing'

class DigiJam:
    def __init__(self, user_id, token):
        self.user_id = user_id
        self.token = token

    def get_song_info(self):

        current_track = self.current_track()
        print(current_track['item']['name'], current_track['item']['artists'])

        artists_json_list = current_track['item']['artists']
        for artist in artists_json_list:
            print(artist['name'])

    def send_request(self, method, url, data):

        headers = {
            "Authorization": f"Bearer {self.token}",
            "Content-Type": "application/json"
        }

        response = requests.request(method, url, headers=headers, data=json.dumps(data) if data else None)

        self.res_code = response.status_code
        self.res_text = response.text

        # Check the response
        if response.status_code == 204:
            print("Command sent successfully.")
            self.get_song_info()
        else:
            print(f"Failed to send command: {response.status_code} - {response.text}")

        return response
    
    def call_refresh(self):

        print("Refreshing token")

        refreshCaller = Refresh()

        self.token = refreshCaller.refresh()
        return self.token

class PlayBack(DigiJam):
    def __init__(self, user_id, token):
        super().__init__(user_id, token)
        
    def song_state(self):

        query = 'https://api.spotify.com/v1/me/player'

        return self.send_request("GET", f"{query}")
        
    def get_playlist_offset(self):

        query = 'https://api.spotify.com/v1/playlists/6k8AkuKcNRyEE2z52r8Vad/tracks'

        response = self.send_request("GET", f"{query}")

        current_track = self.current_track()['item']['id']
        items = response.json()['items']

        offset = 0
        found = False
        for item in items:
            uri = item['track']['id']
            if uri == current_track:
                found = True
                break
            offset += 1

        return found, offset
    
    def play_song(self, playlist):

        query = "https://api.spotify.com/v1/me/player/play"
        
        pos_time = 0
        offset = 0

        if playlist == ALBUM:
            state = self.song_state()
            pos_time = state.json()['progress_ms']
            found, offset = self.get_playlist_offset()
            if not found:
                pos_time = 0
                offset = 0

        body = {
            "context_uri": playlist,
            "offset": {"position": offset},
            "position_ms": pos_time
        }

        self.send_request("PUT", f"{query}", data=json.dumps(body))

    def pause_song(self):
        query = "https://api.spotify.com/v1/me/player/pause"
        
        self.send_request("PUT", f"{query}")

    def next_song(self):
        query = "https://api.spotify.com/v1/me/player/next"

        self.send_request("POST", f"{query}")

    def prev_song(self):
        query = "https://api.spotify.com/v1/me/player/previous"

        self.send_request("POST", f"{query}")

class SongFavorites(DigiJam):
    def __init__(self, user_id, token, name):
        super().__init__(user_id, token)
        self.name = name
        self.id = ""
        self.tracks = ""

    def current_track(self):
        query = 'https://api.spotify.com/v1/me/player/currently-playing'

        return (self.send_request("GET", f"{query}")).json()
    
    def add_favorites(self):

        if self.favorite_playlist_id == '':
            self.create_playlist()

        current_track = self.current_track()
        song_uri = current_track['item']['uri']

        query = f'https://api.spotify.com/v1/playlists/{self.id}/tracks'

        body = {
            "position": 0,
            "uris": [song_uri]
        }
        self.send_request("POST", f"{query}", data=json.dumps(body))

    def get_current_playlists(self):

        query = 'https://api.spotify.com/v1/me/playlists'

        response = self.send_request("GET", f"{query}")

        return response.json()['items']
    
    def create_playlist(self):
        # Create a new playlist

        created = False

        for item in self.get_current_playlists():

            if item['name'] == self.favorite_playlist_name:
                created = True
                self.id = item['id']
                break

        if not created:
            print("Trying to create playlist...")

            query = "https://api.spotify.com/v1/users/{}/playlists".format(
                self.user_id)

            body = json.dumps({
                "name": self.name, "description": "Songs favorited from DigiJam", "public": True
            })

            response = self.send_request("POST", f"{query}", data=json.dumps(body))

            response_json = response.json()

            self.id = response_json["id"]

        return self.id

    def add_to_playlist(self):
        # add all songs to new playlist
        print("Adding songs...")

        self.id = self.create_playlist()

        query = "https://api.spotify.com/v1/playlists/{}/tracks?uris={}".format(
            self.id, self.tracks)

        self.send_request("POST", f"{query}")

DigiJam = DigiJam(spotify_user_id, spotify_token)
player = PlayBack(spotify_user_id, spotify_token)
playlistManager = SongFavorites(spotify_user_id, spotify_token, "DigiJam Favorited")
spotify_token = DigiJam.call_refresh()
playlistManager.create_playlist()


def lambda_handler(event, context):

    if event['action'] == 'play_song':
        player.play_song(ALBUM)
    elif event['action'] == 'pause_song':
        player.pause_song()
    elif event['action'] == 'right':
        player.next_song()
    elif event['action'] == 'left':
        player.prev_song()
    elif event['action'] == 'add_favorite':
        playlistManager.add_favorites()
    elif event['action'] == 'play_favorite':
        player.play_song(f'spotify:playlist:{player.favorite_playlist_id}')
    song = player.current_track()["item"]
    song_name = song["name"]
    song_artist = song["artists"][0]["name"]

    data = {"state": {"desired": {"song_name": song_name, "song_artist": song_artist}}}
    payload_data = json.dumps(data)

    response = client.update_thing_shadow(
        thingName=thing_name,
        payload=payload_data
    )

