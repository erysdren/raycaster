# Raycaster

A simple sector and linedef based raycaster written in C.

## Motivation
Once upon a time when taking part in a gamejam event I wrote a little [pseudo-3D RPG](https://eigen.itch.io/sunless-isle) that fit on a 64x64 screen. It started out as a standard raycaster experiment following [Lode's tutorial](https://lodev.org/cgtutor/raycasting.html) (most well known on the subject probably) but I decided to try and replace regular grid map with lines instead. So instead of checking collisions and intersections *optimally*, you do the opposite and find all lines the ray intersects and draw them in order. The upside is that you can have varying heights of walls and walls behind each other. This project follows that idea but has a bunch of improvements, although the underlying algorithm is still not optimal, since you have to find a lot of line intersections for each column instead of doing what DOOM does, for example. BUT the benefit is a lot simpler code to follow and modify, so yeah.. pros and cons.

## Features
* Basic sector and linedef handling
* Basic level construction: define some polygons and automatically build the sectors
* Sector lighting with distance falloff for walls, floors and ceilings
* Uses OpenMP to render columns in parallel (optional)

## Unfeatures
* Texturing
* Sprites
* ~Lighting~

![image](https://github.com/user-attachments/assets/94be15ab-71d8-4956-b850-2ef8935f49d4)
![image](https://github.com/user-attachments/assets/6a2ae674-7da7-49c7-9dc1-e59675c8c460)
![image](https://github.com/user-attachments/assets/fef181fa-d4b3-49cd-9148-dcaed470c521)
![image](https://github.com/user-attachments/assets/d8273d82-c590-4c58-a8dd-3c396a5b1353)
![image](https://github.com/user-attachments/assets/2dd0107c-3aca-4c2f-8cbf-b8003d274dfd)

## More details
The general concept is to have **sectors** that define floor and ceiling height (and light in the future) and where each sector has some **linedefs** which can have a reference to the sector behind it. You start drawing from the sector the camera is currently in --- for each column you check that sector's visible linedefs for intersections and sort them by distance. If the linedef has no back sector, you draw a full wall segment and terminate that column. If there is a back sector, you draw an upper and lower wall segments based on the floor and ceiling height difference compared to current sector, and then move on the sector behind and repeat. You keep track of sectors that have been visited in each column to avoid cycling.

### Getting started
The library uses [nob.h](https://github.com/tsoding/nob.h) to bootstrap the build command to build the test and demo target.

1. Use `gcc -o build build.c -std=gnu99` to create the builder (or `CC`, depending on your compiler situation)
2. Then run `build demo` or `build tests`
3. Demo application accepts a `-level <number>` argument to try out a couple of levels (0 to 3)
