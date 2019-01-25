import sys
import time
import random

# cursor movement ansi codes
up = "\u001b[{n}A"
down = "\u001b[{n}B"
right = "\u001b[{n}C"
left =" \u001b[{n}D"


# prints the line then moves cursor to the right of the next
def existing():
    print("Creamy tomato soup!")
    time.sleep(1)

# move left and overwrite the last written line
def loading():
    print("Loading...")
    for i in range(0, 100):
        time.sleep(0.05)
        # move cursor left by 1000 positions
        print(u"\u001b[1000D" + str(i + 1) + "%", end="")
        sys.stdout.flush()
    print()

# carriage return is cleaner than moving left a large number of times
def asciibar():
    print("Loading...")
    for i in range(0, 100):
        time.sleep(0.05)
        width = int((i + 1) / 4)
        bar = "[" + "#" * width + " " * (25 - width) + "]"
        print(bar + "\r", end="")
    print()

# randomly advances multiple ascii progress bars
def multiple(num):
    all_progress = [0] * num
    # prepare enough empty lines
    print("\n" * num, end="")
    while any(x < 100 for x in all_progress):
        time.sleep(0.0125)
        # randomly increment one
        unfinished = [(i, v) for (i, v) in enumerate(all_progress) if v < 100]
        index, _ = random.choice(unfinished)
        all_progress[index] += 1
        # move up and re-draw all the bars
        print("\u001b[" + str(num) + "A", end="")
        for progress in all_progress:
            width = int(progress / 4)
            print("[" + "#" * width + " " * (25 - width) + "]")
    
    
if __name__ == "__main__":
    # print("Percent:")
    # loading()

    # print("Ascii:")
    # asciibar()

    print("Multiple:")
    multiple(4)
