# SPDX-FileCopyrightText: 2023 Trevor Beaton for Adafruit Industries
# SPDX-License-Identifier: MIT

import time
import traceback
import supervisor
import displayio
import terminalio
import espnow
import wifi
import os
import rtc
import espidf
import socketpool
import struct
from adafruit_display_text.label import Label
from adafruit_bitmap_font import bitmap_font
from adafruit_matrixportal.matrix import Matrix

class TimerDisplayNode:
    def __init__(self):
        # set the timer length
        self.TIMER_LENGTH_S = 180  # 3 minutes

        # set whether the display colon should blink each second
        self.BLINK = True

        # set whether to display to a terminal instead for debug
        self.DEBUG = False

        # display setup (make a reference to a display object)
        self.matrix = Matrix()
        self.display = self.matrix.display

        # timer drawing setup
        self.group = displayio.Group()  # Create a Group
        self.bitmap = displayio.Bitmap(64, 32, 2)  # Create a bitmap object,width, height, bit depth
        self.color = displayio.Palette(4)  # Create a color palette
        self.color[0] = 0x000000  # black background
        self.color[1] = 0xFF0000  # red
        self.color[2] = 0xFF8C00  # yellow
        self.color[3] = 0x3DEB34  # green

        # create a TileGrid using the Bitmap and Palette
        tile_grid = displayio.TileGrid(self.bitmap, pixel_shader=self.color)
        self.group.append(tile_grid)  # Add the TileGrid to the Group
        self.display.root_group = self.group

        if self.DEBUG:
            self.font = terminalio.FONT
        else:
            self.font = bitmap_font.load_font("/IBMPlexMono-Medium-24_jep.bdf")

        self.clock_label = Label(self.font)

        # flag for whether timer is counting down
        self.timer_running = False

        # reset timer to display full timer length
        self.remaining_time = self.TIMER_LENGTH_S
        self.update_time(self.remaining_time)
        self.group.append(self.clock_label)

        #### ESPNOW Receiver#######

        # C++ side packet structure:
        # // Structure example to send data
        # // Must match the receiver structure
        # typedef struct struct_message {
        #   int systemState; //broadcast the system state
        #   int systemTime; //timer for the system
        # } struct_message;

        # Packet message
        # b'\x04\x00\x00\x00\x07\x00\x00\x00'
        # Packet message type
        # <class 'bytes'>
        # MAC Address
        # b'\xa8B\xe3F\xfe\xf8'
        # Packet Type
        # <class 'ESPNowPacket'>

        self.espnow_packet = espnow.ESPNow()
        CONTROLLER_PEER = espnow.Peer(mac=b'\xa8B\xe3F\xfe\xf8')
        self.espnow_packet.peers.append(CONTROLLER_PEER)

        ###########################

    def read_state(self) -> int:
        '''
        Read the last received ESP-NOW packet and return the match state as an integer
        Return None if packet is empty or Null
        '''
        if (self.espnow_packet):
            packet_bytes = self.espnow_packet.read()
            if (packet_bytes is None):
                return None
            return packet_bytes.msg[0]
        else:
            return None
        
    def send_time(self) -> None:
        '''
        Broadcast an ESP-NOW packet with an updated remaining time in seconds
        '''
        bytes_int_list = [0, 0, 0, 0, self.remaining_time, 0, 0, 0]
        self.espnow_packet.send(bytes(bytes_int_list))

    def update_time(self, remaining_time):
        '''take in current remaining_time, update it, display it, and return new remaining_time'''
        # calculate remaining time in seconds
        seconds = remaining_time % 60
        minutes = remaining_time // 60

        if self.BLINK:
            colon = ":" if (remaining_time % 2) else " "
        else:
            colon = ":"

        self.clock_label.text = "{minutes:02d}{colon}{seconds:02d}".format(
            minutes=minutes, seconds=seconds, colon=colon
        )

        if remaining_time < 11:
            self.clock_label.color = self.color[1] # red text for final ten seconds
        elif remaining_time < 60:
            self.clock_label.color = self.color[2] # yellow text for less than one minute remaining
        elif remaining_time > 60:
            self.clock_label.color = self.color[3] # green text for more than 1 minute remaining

        bbx, bby, bbwidth, bbh = self.clock_label.bounding_box
        # Center the label
        self.clock_label.x = round(self.display.width / 2 - bbwidth / 2)
        self.clock_label.y = self.display.height // 2
        if self.DEBUG:
            print("Label bounding box: {},{},{},{}".format(bbx, bby, bbwidth, bbh))
            print("Label x: {} y: {}".format(self.clock_label.x, self.clock_label.y))

        # decrement remaining time
        remaining_time -= 1
        if remaining_time < 0:
            remaining_time = self.TIMER_LENGTH_S

        return remaining_time

    def start_timer(self):
        '''start the timer counting down'''
        self.timer_running = True

    def pause_timer(self):
        '''stop the timer from counting down'''
        self.timer_running = False

    def reset_timer(self):
        '''reset the remaining_time and update display'''
        self.timer_running = False
        self.remaining_time = self.TIMER_LENGTH_S
        self.update_time(self.remaining_time)

    def tick_timer_s(self):
        '''count the timer down by one second and update display'''
        if (self.timer_running):
            self.remaining_time = self.update_time(self.remaining_time)

    def tick_match_fsm(self):
        '''run the timer state machine'''
        state = self.read_state()
        if (state == 0):    # idle state
            self.reset_timer()
        elif (state == 1):  # load-in state
            pass
        elif (state == 2):  # read for battle state
            pass
        elif (state == 3):  # countdown state
            pass
        elif (state == 4):  # match state
            self.start_timer()
        elif (state == 5):  # one minute remaining state
            pass
        elif (state == 6):  # 10 seconds remaining state
            pass
        elif (state == 7):  # match end state
            self.pause_timer()
        elif (state == 8):  # ko confirm state
            pass
        elif (state == 9):  # ko + tap out state
            self.pause_timer()
        elif (state == 10): # e-stop state
            self.pause_timer()
        elif (state == 11): # pause state
            self.pause_timer()
        else:               # invalid state / no received packet
            pass

if __name__ == "__main__":
    node = TimerDisplayNode()
    prev_time = time.monotonic_ns()

    # main loop
    while True:
        # receive the state and run the state machine
        node.tick_match_fsm()

        # each second, tick the timer and broadcast the remaining seconds
        current_time = time.monotonic_ns() # unix time in int seconds
        if (current_time > (prev_time + 1e9)):
            prev_time = current_time

            # tick the timer
            node.tick_timer_s()

            # broadcast the remaining seconds
            # node.send_time()