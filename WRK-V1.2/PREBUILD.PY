import os

cwd = os.getcwd()

for root, dirs, files in os.walk(cwd):
    for dir in dirs:
        if dir.lower() == "build":
            os.makedirs(os.path.join(root, dir, "OBJI386"), exist_ok = True)
            os.makedirs(os.path.join(root, dir, "OBJAMD64"), exist_ok = True)

os.makedirs(os.path.join(cwd, "BASE", "NTOS", "BUILD", "EXE"), exist_ok = True)
