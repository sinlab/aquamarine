# built-in modules
import sys
import os
import glob
import argparse
# requirement modules
import cv2

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

'''
Extract only frame files from the folder specified by dirpath. Sort by file name.
'''
def getFrameFiles(dirpath:str):
    
    # List all files
    allfiles = glob.glob(dirpath + "\\*")

    # Count the number of files per extension
    extdict = {}
    for path in allfiles:
        name, ext = os.path.splitext(path)
        if ext in extdict:
            extdict[ext] += 1   # Count up if it exists in dict
        else:
            extdict[ext] = 1    # Add if not present in dict
    
    # Get the largest number of file extensions in a folder
    extdict = sorted(extdict.items(), key = lambda x: x[1], reverse = True)
    mainext = extdict[0][0]
    # List only frame files
    framefiles = glob.glob(dirpath + "\\*" + mainext)

    return framefiles

'''
Concatenate frame files in the folder specified by dirpath to generate a movie.
'''
def makeVideoFromDir(dirpath:str, fps=30, dstsize=(0,0)):

    # List the file path of the frame
    framefiles = getFrameFiles(dirpath)

    if(len(framefiles) < 0):
        return

    # Open the first frame to get the frame size
    firstframe = cv2.imread(framefiles[0])
    # Resize if specified
    if(0 < dstsize[0] and 0 < dstsize[1]):
        firstframe = cv2.resize(firstframe, dstsize)
    h = firstframe.shape[0]
    w = firstframe.shape[1]

    enc = VideoEncoder(dirpath + ".mp4", w, h, fps) 

    for path in framefiles:
        frame = cv2.imread(path)
        
        # Resize if specified
        if(0 < dstsize[0] and 0 < dstsize[1]):
            frame = cv2.resize(frame, dstsize)

        enc.write(frame)

'''
Dump all frames of the video specified in path to a folder.
'''
def makeFramesToDir(path:str, dstsize=(0,0)):

    # Generate output destination folder
    dstdir = path + ".frames"
    os.makedirs(dstdir, exist_ok=True)

    dec = VideoDecoder(path)

    for f in range(0, dec.frames):
        ret, frame = dec.read()

        if(ret):
            # Generate file path
            fstr = '{:08d}'.format(f)
            outpath = dstdir + "\\" + fstr + ".png"

            # Resize if specified
            if(0 < dstsize[0] and 0 < dstsize[1]):
                frame = cv2.resize(frame, dstsize)

            cv2.imwrite(outpath, frame)
        

if __name__ == "__main__":
    
    # Define an argument parser
    p = argparse.ArgumentParser()
    p.add_argument("moviefile_or_framesdir")
    p.add_argument("-f", "--fps", default = 30, type=float)
    p.add_argument("-s", "--size", default = (0, 0), nargs='+', type=int)

    # Parse arguments
    args = p.parse_args()
    input = args.moviefile_or_framesdir
    fps = args.fps
    dstsize = args.size

    if(os.path.isfile(input)):
        makeFramesToDir(input, dstsize)
    else:
        makeVideoFromDir(input, fps, dstsize)
