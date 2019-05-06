# built-in modules
import sys
import os
import glob
import argparse
# requirement modules
import cv2
import moviepy.editor as mp

class VideoDecoder:
    def __init__(self, path:str):
        self.dec    = cv2.VideoCapture(path)
        self.fps    = self.dec.get(cv2.CAP_PROP_FPS)
        self.h      = int(self.dec.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.w      = int(self.dec.get(cv2.CAP_PROP_FRAME_WIDTH))
        self.frames = int(self.dec.get(cv2.CAP_PROP_FRAME_COUNT))
        self.count  = 0
    def __del__(self):
        self.dec.release()
    def read(self):
        self.count += 1
        return self.dec.read()

class VideoEncoder:
    def __init__(self, path:str, w:int, h:int, fps):
        self.enc    = cv2.VideoWriter(path, cv2.VideoWriter_fourcc(*'mp4v'), fps, (w, h))
        self.count  = 0
    def __del__(self):
        self.enc.release()
    def write(self, bgr):
        self.count += 1
        self.enc.write(bgr)

def ExtractAudioFile(srcmovie:str, dstaudio:str, dstcodec:str = ""):
    clip_input = mp.VideoFileClip(srcmovie).subclip()
    if(len(dstcodec) == 0):
        clip_input.audio.write_audiofile(dstaudio)
    else:
        clip_input.audio.write_audiofile(dstaudio, codec = dstcodec)

def MuxVideoFile(srcvideo:str, srcaudio:str, dstvideo:str):
    clip_output = mp.VideoFileClip(srcvideo).subclip()
    clip_output.write_videofile(dstvideo, audio = srcaudio)

if __name__ == "__main__":
    
    # Define an argument parser
    p = argparse.ArgumentParser()
    p.add_argument("moviefile")

    # Parse arguments
    args = p.parse_args()
    input = args.moviefile
    output = input + ".out.mp4"

    # Extract audio
    tempAudioStream = "__tempaudio__.aac"
    ExtractAudioFile(input, tempAudioStream, "aac")

    # Video processing
    tempVideoStream = "__tempvideo__.mp4"
    dec = VideoDecoder(input)
    enc = VideoEncoder(tempVideoStream, dec.w, dec.h, dec.fps)
    # Something to do for each frames...
    for f in range(0, dec.frames):
        ret, frame = dec.read()
        enc.write(cv2.flip(frame, 1))
    del dec
    del enc

    # Mux
    MuxVideoFile(tempVideoStream, tempAudioStream, output)

    # Delete temp files
    os.remove(tempAudioStream)
    os.remove(tempVideoStream)