
import pygame
import sys
import numpy as np
import random, time

class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y
        self.children = []
    def __add__(self, _p):
        return Point(self.x + _p.x, self.y + _p.y)
    def __sub__(self, _p):
        return Point(self.x - _p.x, self.y - _p.y)
    def __truediv__(self, mag):
        return Point(self.x/mag, self.y/mag)
    def __mul__(self, mag):
        return Point(self.x*mag, self.y*mag)
    def add_child(self, p):
        self.children.append(p)

def L2_squared(p1, p2):
    return (p1.x - p2.x)**2 + (p1.y - p2.y)**2

def sample_point(xub, xlb, yub, ylb):
    return Point(random.random()*(xub - xlb) + xlb, random.random()*(yub - ylb) + ylb)

def get_nearest(S, p, df):
    min_d = df(p, S[0])
    min_ind = 0
    for i in range(len(S)):
        if df(S[i], p) < min_d:
            min_d = df(S[i], p)
            min_ind = i
    return min_ind, min_d**0.5

def enforce_dist_limits(p_new, p_nearest, mindist, maxdist, dist):
    if dist <= maxdist:
        return p_new
    dir_vec = (p_new - p_nearest)/dist
    dir_vec = dir_vec*maxdist
    return p_nearest + dir_vec

def init_rrt():
    rrt = []
    rrt.append(Point(400, 300))
    return rrt

def grow_rrt(S, mindist, maxdist):
    p_new = sample_point(800, 0, 600, 0)
    closest_ind, dist = get_nearest(S, p_new, L2_squared)
    p_closest = S[closest_ind]
    if dist == 0:
        return -1
    p_new = enforce_dist_limits(p_new, p_closest, mindist, maxdist, dist)
    p_closest.add_child(p_new)
    S.append(p_new)
    return p_closest, p_new

def main():
    pygame.init()
    screen = pygame.display.set_mode((800, 600))

    S = init_rrt()
    while True:
        p1, p2 = grow_rrt(S, 10, 20)
        if p2 is not None :
            pygame.draw.line(screen, (128, 100, 0), (p1.x, p1.y), (p2.x, p2.y), 1)
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                sys.exit()
        pygame.display.flip()

if __name__ == "__main__":
    main()
