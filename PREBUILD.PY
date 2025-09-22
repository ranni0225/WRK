import os

cwd = os.getcwd()

for root, dirs, files in os.walk(cwd):
    for dir in dirs:
        if dir.lower() == "build":
            os.makedirs(os.path.join(root, dir, "OBJI386"), exist_ok = True)
            os.makedirs(os.path.join(root, dir, "OBJAMD64"), exist_ok = True)

os.makedirs(os.path.join(cwd, "WRK-V1.2", "BASE", "NTOS", "BUILD", "EXE"), exist_ok = True)

os.makedirs(os.path.join(cwd, "ProjectOZ", "BasicOZ", "OBJI386"), exist_ok = True)
os.makedirs(os.path.join(cwd, "ProjectOZ", "SPACE", "OBJI386"), exist_ok = True)
os.makedirs(os.path.join(cwd, "ProjectOZ", "runx86"), exist_ok = True)
os.makedirs(os.path.join(cwd, "ProjectOZ", "files", "i386"), exist_ok = True)
