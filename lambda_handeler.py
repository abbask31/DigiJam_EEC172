import requests
from submission.secrets import spotify_token, spotify_user_id
from submission.refresh import Refresh
import json
import boto3

ALBUM = "spotify:playlist:6k8AkuKcNRyEE2z52r8Vad"

ACCESS_KEY = 'YOUR_ACCESS_KEY'
SECRET_KEY = 'YOUR_SECRET_KEY'

client = boto3.client("iot-data", aws_access_key_id=ACCESS_KEY, aws_secret_access_key=SECRET_KEY,
                      region_name="us-west-2")
thing_name = 'sns_thing'


class PlaySong:
    def __init__(self):
        self.user_id = spotify_user_id
        self.spotify_token = spotify_token
        # self.discover_weekly_id = discover_weekly_id
        self.tracks = ""
        self.new_playlist_id = ""
        self.res_code = ''
        self.res_text = ''
        self.favorite_playlist_id = ''
        self.favorite_playlist_name = 'DigiJam Favorited'

    def song_state(self):

        query = 'https://api.spotify.com/v1/me/player'

        response = requests.put(query)

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }

        response = requests.get(f"{query}", headers=headers)
        self.res_code = response.status_code
        self.res_text = response.text

        return response

    def current_track(self):
        query = 'https://api.spotify.com/v1/me/player/currently-playing'

        response = requests.put(query)

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }

        response = requests.get(f"{query}", headers=headers)

        self.res_code = response.status_code
        self.res_text = response.text

        return response.json()

    def get_song_info(self):

        current_track = self.current_track()
        print(current_track['item']['name'], current_track['item']['artists'])

        artists_json_list = current_track['item']['artists']
        artist_list = []
        for artist in artists_json_list:
            print(artist['name'])

    def get_current_playlists(self):

        query = 'https://api.spotify.com/v1/me/playlists'

        response = requests.put(query)

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }

        response = requests.get(f"{query}", headers=headers)

        return response.json()['items']

    def add_favorites(self):

        if self.favorite_playlist_id == '':
            self.create_playlist()

        current_track = self.current_track()
        song_uri = current_track['item']['uri']
        print(self.favorite_playlist_id)

        print(song_uri)
        query = f'https://api.spotify.com/v1/playlists/{self.favorite_playlist_id}/tracks'

        response = requests.put(query)

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }

        body = {
            "position": 0,
            "uris": [song_uri]
        }
        response = requests.post(f"{query}", headers=headers, data=json.dumps(body))

        if response.status_code == 201:
            print("Song previoused successfully.")
        else:
            print(f"Failed to previous playback: {response.status_code} - {response.text}")

    def get_playlist_offset(self):

        query = 'https://api.spotify.com/v1/playlists/6k8AkuKcNRyEE2z52r8Vad/tracks'

        response = requests.put(query)

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }

        response = requests.get(f"{query}", headers=headers)

        self.res_code = response.status_code
        self.res_text = response.text

        current_track = self.current_track()['item']['id']
        print(current_track)
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

        print(playlist)

        response = requests.put(query)

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }
        pos_time = 0
        offset = 0

        if playlist == ALBUM:
            state = self.song_state()
            print(state.json())
            pos_time = state.json()['progress_ms']
            found, offset = self.get_playlist_offset()
            if not found:
                pos_time = 0
                offset = 0
            print(state)
        print(offset)
        # The body of the request
        # Replace the URIs with the ones for the tracks you want to play
        body = {
            "context_uri": playlist,
            "offset": {"position": offset},
            "position_ms": pos_time
            # Optional: Add context_uri, offset, position_ms as needed
        }

        # Make the PUT request
        response = requests.put(f"{query}", headers=headers, data=json.dumps(body))

        self.res_code = response.status_code
        self.res_text = response.text

        # Check the response
        if response.status_code == 204:
            print("Playback started successfully.")
            self.get_song_info()
        else:
            print(f"Failed to start playback: {response.status_code} - {response.text}")

    def pause_song(self):
        query = "https://api.spotify.com/v1/me/player/pause"

        response = requests.put(query, )

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }
        response = requests.put(f"{query}", headers=headers)

        self.res_code = response.status_code
        self.res_text = response.text

        # Check the response
        if response.status_code == 204:
            print("Playback paused successfully.")
        else:
            print(f"Failed to pause playback: {response.status_code} - {response.text}")

    def next_song(self):
        query = "https://api.spotify.com/v1/me/player/next"

        response = requests.post(query)

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }
        response = requests.post(f"{query}", headers=headers)

        self.res_code = response.status_code
        self.res_text = response.text

        # Check the response
        if response.status_code == 204:
            print("Song skipped successfully.")
        else:
            print(f"Failed to skip playback: {response.status_code} - {response.text}")

    def prev_song(self):
        query = "https://api.spotify.com/v1/me/player/previous"

        response = requests.post(query)

        headers = {
            "Authorization": f"Bearer {spotify_token}",
            "Content-Type": "application/json"
        }
        response = requests.post(f"{query}", headers=headers)

        self.res_code = response.status_code
        self.res_text = response.text

        # Check the response
        if response.status_code == 204:
            print("Song previoused successfully.")
        else:
            print(f"Failed to previous playback: {response.status_code} - {response.text}")

    def create_playlist(self):
        # Create a new playlist

        created = False
        array = self.get_current_playlists()
        print(array)

        for item in self.get_current_playlists():

            if item['name'] == self.favorite_playlist_name:
                print('exists')
                print(item['name'])
                created = True
                self.favorite_playlist_id = item['id']
                break

        if not created:
            print("Trying to create playlist...")

            query = "https://api.spotify.com/v1/users/{}/playlists".format(
                spotify_user_id)

            request_body = json.dumps({
                "name": self.favorite_playlist_name, "description": "Songs favorited from DigiJam", "public": True
            })

            response = requests.post(query, data=request_body, headers={
                "Content-Type": "application/json",
                "Authorization": "Bearer {}".format(self.spotify_token)
            })

            self.res_code = response.status_code
            self.res_text = response.text

            response_json = response.json()

            self.favorite_playlist_id = response_json["id"]

    def add_to_playlist(self):
        # add all songs to new playlist
        print("Adding songs...")

        self.new_playlist_id = self.create_playlist()

        query = "https://api.spotify.com/v1/playlists/{}/tracks?uris={}".format(
            self.new_playlist_id, self.tracks)

        response = requests.post(query, headers={"Content-Type": "application/json",
                                                 "Authorization": "Bearer {}".format(self.spotify_token)})

        self.res_code = response.status_code
        self.res_text = response.text

        print(response.json)

    def call_refresh(self):

        print("Refreshing token")

        refreshCaller = Refresh()

        self.spotify_token = refreshCaller.refresh()
        spotify_token = self.spotify_token
        # self.find_songs()
        return spotify_token


player = PlaySong()
spotify_token = player.call_refresh()
player.create_playlist()


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
        player.add_favorites()
    elif event['action'] == 'play_favorite':
        player.play_song(f'spotify:playlist:{player.favorite_playlist_id}')
    song = player.current_track()["item"]
    song_name = song["name"]
    song_artist = song["artists"][0]["name"]
    print(song_name)
    print(song_artist)

    data = {"state": {"desired": {"song_name": song_name, "song_artist": song_artist}}}
    payload_data = json.dumps(data)

    response = client.update_thing_shadow(
        thingName=thing_name,
        payload=payload_data
    )

