/************************************************
 *         xlaby - daemonic X labyrinth         *
 * Copyright (c) P.Horvai (peter.horvai@ens.fr) *
 *           and D.Madore (david.madore@ens.fr) *
 *           Version 2.01 - 1997 Dec 01         *
 ************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/*
 * (Please consider the following text to be in very large capitals.)
 *  This program comes with absolutely no warranty of any kind (see the
 *  GNU General Public License for details if you don't know what this
 *  means; when I say no warrantee, I really mean _none_).
 *  In particular, you run this program at your own risk, and any loss
 *  of mental capabilities or precious computer data which may result
 *  as a consequence of running this program is entirely your fault.
 */

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

int subx, suby;   /* The current cell                 */
char rungame;     /* Is there something to do?        */
char nogame;      /* Was --noplay specified?          */
char noquit;      /* Was --noquit specified?          */
char grabkbd;     /* Was --grabkbd specified?         */
char slowbuild;   /* Was --build specified?           */
char quantum;     /* Was --quantum specified?         */
char visibility;  /* 0=normal, 1=discover, 2=blind    */
char mazetype;    /* 0=treemaze, 1=floodmaze          */
int mazesize;     /* 0=very small, ..., 9=huge        */
int wposx, wposy; /* Main window position on screen   */
char grabbing;    /* Is the pointer being grabbed     */

#define MAXHORIZ 150                      /* Size not to exceed        */
#define MAXVERT 120                       /* Size not to exceed        */
int cellwidth;                            /* Width of a maze cell      */
int cellheight;                           /* Height of a maze cell     */
int cellhoriz;                            /* Number of columns in maze */
int cellvert;                             /* Number of lines in maze   */
#define WINWIDTH (cellwidth * cellhoriz)  /* Maze window width         */
#define WINHEIGHT (cellheight * cellvert) /* Maze window height        */
#define MINSCREENWIDTH (WINWIDTH + 15)    /* Minimal screen width      */
#define MINSCREENHEIGHT (WINHEIGHT + 12)  /* Minimal screen height     */
/*  The following variable is unfortunately hardcoded in several places... */
#define MAZEGOALS 5 /* (counting start)          */
#define QPROB 6000  /* Quantum leap prob. (ppm)  */

Display *dpy;     /* Output display                   */
char *display;    /* Output display name              */
int scn;          /* Screen number used               */
Screen *screen;   /* Corresponding screen structure   */
int screenwidth,  /* Screen width                     */
    screenheight; /* Screen height                    */
Window root,      /* Root window of that screen       */
    myw,          /* The maze window                  */
    subw;         /* The mobile window                */
GC gc;            /* Graphic context used             */
Colormap cmap;    /* Colormap used                    */
char owncmap;     /* Are we using our own colormap    */
char pmexists;    /* Is the following variable valid? */
Pixmap pm;        /* Backup store of maze window      */
Cursor curs;      /* Cursor used                      */

/* Various colors */
XColor c_back, c_wall, c_goal[MAZEGOALS];

char *wname = "X Labyrinth - the aMAZEment"; /* The window name */

char wallh[MAXHORIZ][MAXVERT]; /* Horizontal walls of the maze           */
char wallv[MAXHORIZ][MAXVERT]; /* Vertical walls of the maze             */
/*  0 means no wall, 1 means a wall, and 2 means a "known" wall */

int reached; /* Last goal reached                */
struct {
  int x;
  int y;
} goals[MAZEGOALS]; /* Position of goals                */

#define MIN(x, y) (((y) < (x)) ? (y) : (x))

long int garbage;
/*
 * The above variable is used to dump all values which this stupid
 * X lib insists on returning and which I don't care about.
 */

void quit(int exitcode)
/* You'll never guess what this one does */
{
  XCloseDisplay(dpy);
  exit(exitcode);
}

void refresh(int x, int y, unsigned int wd, unsigned int hg);
void redraw(int cx, int cy, int ch, int cv);
void checkexpose(void);

void genmaze1(void)
/* Generates the maze using method 1 (tree maze) */
/* The maze is calculated by growing trees from the borders of the window
 * (and also from the center, so as to make diagonal crossing harder).
 * As usual, I chose the simple and inefficient method. Ugh... */
/*  You can decrease but not increase the following constant */
#define MAZESTEPS                                                              \
  ((cellhoriz - 1) * (cellvert - 1) - 1) /* Number of walls in maze   */
{
  int i, j, k, x1, y1, x2, y2;
  char point[MAXHORIZ + 1][MAXVERT + 1];
  goals[0].x = 0;
  goals[0].y = 0;
  goals[1].x = cellhoriz - 1;
  goals[1].y = 0;
  goals[2].x = 0;
  goals[2].y = cellvert - 1;
  goals[3].x = cellhoriz - 1;
  goals[3].y = cellvert - 1;
  goals[4].x = random() % cellhoriz;
  goals[4].y = random() % cellvert;
  for (i = 0; i <= cellhoriz; i++)
    for (j = 0; j <= cellvert; j++) {
      point[i][j] = (i == 0) || (j == 0) || (i == cellhoriz) || (j == cellvert);
    }
  point[cellhoriz / 2][cellvert / 2] =
      1; /* This seems to make things more diff^H^H^H^H^H^H^H^H^Hless trivial */
  if (slowbuild) {
    redraw(0, 0, cellhoriz, cellvert);
    refresh(0, 0, WINWIDTH, WINHEIGHT);
  }
  for (k = 0; k < MAZESTEPS; k++) {
    if (slowbuild)
      checkexpose();
    do {
      x1 = random() % (cellhoriz + 1);
      y1 = random() % (cellvert + 1);
      do {
        switch (i = random() % 4) {
        case 0:
          x2 = x1 + 1;
          y2 = y1;
          break;
        case 1:
          x2 = x1 - 1;
          y2 = y1;
          break;
        case 2:
          x2 = x1;
          y2 = y1 + 1;
          break;
        case 3:
          x2 = x1;
          y2 = y1 - 1;
          break;
        }
      } while (
          !((x2 >= 0) && (y2 >= 0) && (x2 <= cellhoriz) && (y2 <= cellvert)));
    } while (point[x1][y1] == point[x2][y2]);
    point[x1][y1] = 1;
    point[x2][y2] = 1;
    switch (i) {
    case 0:
      wallh[x1][y1] = 1;
      break;
    case 1:
      wallh[x2][y2] = 1;
      break;
    case 2:
      wallv[x1][y1] = 1;
      break;
    case 3:
      wallv[x2][y2] = 1;
      break;
    }
    if (slowbuild) {
      redraw(x1, y1, 1, 1);
      refresh(x1 * cellwidth, y1 * cellheight, cellwidth, cellheight);
      redraw(x2, y2, 1, 1);
      refresh(x2 * cellwidth, y2 * cellheight, cellwidth, cellheight);
    }
  }
}

void genmaze2(void)
/* Generates the maze using method 2 (flood maze) */
/* This time, the path is generated, using a self-avoiding random walk, and it
 * determines the walls. */
{
  int i, j, k, l, ol, ii, jj;
  char vis[MAXHORIZ][MAXVERT];
  struct {
    int x;
    int y;
  } path[MAXHORIZ * MAXVERT]; /* Hope the stack is big enough! */
  int pathlen;
  goals[0].x = 0;
  goals[0].y = 0;
  goals[1].x = cellhoriz - 1;
  goals[1].y = 0;
  goals[2].x = 0;
  goals[2].y = cellvert - 1;
  goals[3].x = cellhoriz - 1;
  goals[3].y = cellvert - 1;
  goals[4].x = random() % cellhoriz;
  goals[4].y = random() % cellvert;
  for (i = 0; i < cellhoriz; i++)
    for (j = 0; j < cellvert; j++) {
      vis[i][j] = 0;
      wallh[i][j] = (j > 0);
      wallv[i][j] = (i > 0);
    }
  if (slowbuild) {
    redraw(0, 0, cellhoriz, cellvert);
    refresh(0, 0, WINWIDTH, WINHEIGHT);
  }
  pathlen = 1;
  path[0].x = cellhoriz / 2;
  path[0].y = cellvert / 2;
  vis[path[0].x][path[0].y] = 1;
  ol = 0;
  while (1) {
    if (slowbuild)
      checkexpose();
    while (pathlen) {
      i = path[pathlen - 1].x;
      j = path[pathlen - 1].y;
      if (((i > 0) && !vis[i - 1][j]) || ((j > 0) && !vis[i][j - 1]) ||
          ((i < cellhoriz - 1) && !vis[i + 1][j]) ||
          ((j < cellvert - 1) && !vis[i][j + 1]))
        break;
      pathlen--;
    }
    if (!pathlen)
      break;
    do {
      l = random() % 5;
      if (l >= 4)
        l = ol; /* There was a bug here prior to v.1.4.1 */
      switch (l) {
      case 0:
        ii = i + 1;
        jj = j;
        break;
      case 1:
        ii = i - 1;
        jj = j;
        break;
      case 2:
        ii = i;
        jj = j + 1;
        break;
      case 3:
        ii = i;
        jj = j - 1;
        break;
      }
    } while ((ii < 0) || (jj < 0) || (ii >= cellhoriz) || (jj >= cellvert) ||
             (vis[ii][jj]));
    ol = l;
    path[pathlen].x = ii;
    path[pathlen].y = jj;
    pathlen++;
    vis[ii][jj] = 1;
    switch (l) {
    case 0:
      wallv[ii][jj] = 0;
      break;
    case 1:
      wallv[i][j] = 0;
      break;
    case 2:
      wallh[ii][jj] = 0;
      break;
    case 3:
      wallh[i][j] = 0;
      break;
    }
    if (slowbuild) {
      redraw(i, j, 1, 1);
      refresh(i * cellwidth, j * cellheight, cellwidth, cellheight);
      redraw(ii, jj, 1, 1);
      refresh(ii * cellwidth, jj * cellheight, cellwidth, cellheight);
    }
  }
}

void genmaze3(void)
/* Generates the maze using method 3 (chain maze) */
/* The difference with the method 2 is that when the path is blocked,
 * it tries to branch from the start rather than from the end */
{
  int i, j, k, l, ol, ii, jj;
  char vis[MAXHORIZ][MAXVERT];
  struct {
    int x;
    int y;
  } path[MAXHORIZ * MAXVERT]; /* Hope the stack is big enough! */
  int pathstart, pathcur, pathend;
  goals[0].x = 0;
  goals[0].y = 0;
  goals[1].x = cellhoriz - 1;
  goals[1].y = 0;
  goals[2].x = 0;
  goals[2].y = cellvert - 1;
  goals[3].x = cellhoriz - 1;
  goals[3].y = cellvert - 1;
  goals[4].x = random() % cellhoriz;
  goals[4].y = random() % cellvert;
  for (i = 0; i < cellhoriz; i++)
    for (j = 0; j < cellvert; j++) {
      vis[i][j] = 0;
      wallh[i][j] = (j > 0);
      wallv[i][j] = (i > 0);
    }
  if (slowbuild) {
    redraw(0, 0, cellhoriz, cellvert);
    refresh(0, 0, WINWIDTH, WINHEIGHT);
  }
  pathstart = 0;
  pathcur = 0;
  pathend = 1;
  path[0].x = cellhoriz / 2;
  path[0].y = cellvert / 2;
  vis[path[0].x][path[0].y] = 1;
  ol = 0;
  while (1) {
    if (slowbuild)
      checkexpose();
    while (pathend > pathstart) {
      i = path[pathcur].x;
      j = path[pathcur].y;
      if (((i > 0) && !vis[i - 1][j]) || ((j > 0) && !vis[i][j - 1]) ||
          ((i < cellhoriz - 1) && !vis[i + 1][j]) ||
          ((j < cellvert - 1) && !vis[i][j + 1]))
        break;
      pathcur = ++pathstart;
    }
    if (pathstart >= pathend)
      break;
    do {
      l = random() % 5;
      if (l >= 4)
        l = ol; /* There was a bug here prior to v.1.4.1 */
      switch (l) {
      case 0:
        ii = i + 1;
        jj = j;
        break;
      case 1:
        ii = i - 1;
        jj = j;
        break;
      case 2:
        ii = i;
        jj = j + 1;
        break;
      case 3:
        ii = i;
        jj = j - 1;
        break;
      }
    } while ((ii < 0) || (jj < 0) || (ii >= cellhoriz) || (jj >= cellvert) ||
             (vis[ii][jj]));
    ol = l;
    path[pathend].x = ii;
    path[pathend].y = jj;
    pathcur = pathend++;
    vis[ii][jj] = 1;
    switch (l) {
    case 0:
      wallv[ii][jj] = 0;
      break;
    case 1:
      wallv[i][j] = 0;
      break;
    case 2:
      wallh[ii][jj] = 0;
      break;
    case 3:
      wallh[i][j] = 0;
      break;
    }
    if (slowbuild) {
      redraw(i, j, 1, 1);
      refresh(i * cellwidth, j * cellheight, cellwidth, cellheight);
      redraw(ii, jj, 1, 1);
      refresh(ii * cellwidth, jj * cellheight, cellwidth, cellheight);
    }
  }
}

void maze(void) {
  fprintf(stderr, "Generating maze...");
  if (mazetype == 1)
    genmaze2();
  else if (mazetype == 2)
    genmaze3();
  else
    genmaze1();
  reached = 0;
  subx = goals[0].x;
  suby = goals[0].y;
  fprintf(stderr, "done.\n");
}

void redraw(int cx, int cy, int ch, int cv)
/* Draws pm - in other words, redraws part of the maze window without
 * actually putting it on the screen (that is done by the next procedure) */
/* (cx,cy) is the first cell to be redrawn, ch is the horizontal number
 * of cells to redraw, and cv the vertical number */
/* Therefore: redraw(0,0,cellhoriz,cellvert) redraws everything, and
 * redraw(x,y,1,1) redraws exactly cell (x,y) */
{
  int i, j, cmx, cmy;
  if (!pmexists) {
    pm = XCreatePixmap(dpy, myw, WINWIDTH, WINHEIGHT, DefaultDepth(dpy, scn));
    pmexists = 1;
  }
  cmx = cx + ch;
  cmy = cy + cv;
  if (cx < 0)
    cx = 0;
  if (cy < 0)
    cy = 0;
  if (cmx > cellhoriz)
    cmx = cellhoriz;
  if (cmy > cellvert)
    cmy = cellvert;
  ch = cmx - cx;
  cv = cmy - cy;
  XSetForeground(dpy, gc, c_back.pixel);
  XFillRectangle(dpy, pm, gc, cx * cellwidth, cy * cellheight, ch * cellwidth,
                 cv * cellheight);
  XSetForeground(dpy, gc, c_wall.pixel);
  for (i = cx; i < cmx; i++)
    for (j = cy; j < cmy; j++) {
      if (wallh[i][j] > visibility)
        XFillRectangle(dpy, pm, gc, i * cellwidth, j * cellheight, cellwidth,
                       1);
      if (wallv[i][j] > visibility)
        XFillRectangle(dpy, pm, gc, i * cellwidth, j * cellheight, 1,
                       cellheight);
    }
  for (i = MAZEGOALS - 1; i > reached; i--)
    if ((goals[i].x >= cx) && (goals[i].y >= cy) && (goals[i].x < cmx) &&
        (goals[i].y < cmy)) {
      XSetForeground(dpy, gc, c_goal[i].pixel);
      XFillRectangle(dpy, pm, gc, goals[i].x * cellwidth + 2,
                     goals[i].y * cellheight + 2, cellwidth - 3,
                     cellheight - 3);
    }
}

void refresh(int x, int y, unsigned int wd, unsigned int hg)
/* Refreshes the indicated area of the maze window (copies it from the
 * pixmap pm to the screen) */
/* This time, coordinates are given in PIXELS, and not CELLS */
/* Therefore: refresh(0,0,WINWIDTH,WINHEIGHT) refreshes everything, and
 * refresh(x*cellwidth,y*cellheight,cellwidth,cellheight) refreshes the
 * cell (x,y) */
{
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x + wd > WINWIDTH)
    wd = WINWIDTH - x;
  if (y + hg > WINHEIGHT)
    hg = WINHEIGHT - y;
  XCopyArea(dpy, pm, myw, gc, x, y, wd, hg, x, y);
}

void checkexpose(void)
/* Checks for an Expose event and, if appropriate, refreshes the window */
{
  XEvent evt;
  if (!XCheckTypedEvent(dpy, Expose, &evt))
    return;
  if ((evt.xexpose.x < WINWIDTH) && (evt.xexpose.y < WINHEIGHT)) {
    refresh(evt.xexpose.x, evt.xexpose.y,
            MIN(evt.xexpose.width, WINWIDTH - evt.xexpose.x),
            MIN(evt.xexpose.height, WINHEIGHT - evt.xexpose.y));
  }
}

void banner(void)
/* Display program banner to stderr */
{
  fprintf(stderr, "************************************************\n");
  fprintf(stderr, "*         xlaby - daemonic X labyrinth         *\n");
  fprintf(stderr, "* Copyright (c) P.Horvai (peter.horvai@ens.fr) *\n");
  fprintf(stderr, "*           and D.Madore (david.madore@ens.fr) *\n");
  fprintf(stderr, "*           Version 2.01 - 1997 Dec 01         *\n");
  fprintf(stderr, "************************************************\n");
}

void printwarning(void)
/* Pring the warning message */
{
  fprintf(stderr,
          "WARNING: running  xlaby --play --noquit --grabkbd  will get you\n");
  fprintf(stderr,
          "in a situation where YOU CANNOT QUIT the game unless by winning\n");
  fprintf(
      stderr,
      "(or by shutting down the X server if you have some \"magic keys\"\n");
  fprintf(stderr, "to do it, or by killing xlaby from another terminal).\n");
  /* And of course, that is exactly what people are expected to do >;-) */
}

void printhelp(char *progname)
/* Print help message */
{
  banner();
  fprintf(stderr, "Type %s --play to start the game. HOWEVER, you are\n",
          progname);
  fprintf(stderr, "strongly advised not to do so until you have read\n");
  fprintf(stderr, "both the game instructions and the warranty.\n");
  fprintf(stderr, "Usage: %s [OPTION]...\n", progname);
  fprintf(stderr, "  -b, --blind     run in blind mode\n");
  fprintf(stderr, "      --build     slow build (so you can see it)\n");
  fprintf(stderr,
          "  -c, --chain     use a chain maze (most difficult, it seems)\n");
  fprintf(stderr, "      --copying   display copying info and exit\n");
  fprintf(stderr, "  -d, --discover  run in \"discover\" (semi-blind) mode\n");
  fprintf(stderr,
          "  -display NAME   set display name (--display is also legal)\n");
  fprintf(stderr, "      --grabkbd   grab the keyboard\n");
  fprintf(stderr,
          "  -f, --flood     use a flood maze; this is longer and harder\n");
  fprintf(stderr,
          "  -h, --help      display this message to stderr and exit\n");
  fprintf(stderr, "  -i, --info      display game instructions and exit\n");
  fprintf(stderr,
          "      --noplay    do not play the game (just admire the maze)\n");
  fprintf(stderr, "      --noquit    disable quit key\n");
  fprintf(stderr,
          "  -p, --play      play the game (read instructions first!)\n");
  fprintf(stderr, "      --pos X Y   force window position to X Y\n");
  fprintf(stderr, "  -q, --quantum   run in quantum mode (try it and see)\n");
  fprintf(stderr,
          "  -s, --size NUM  maze size (0=smallest, 5=default, 9=largest)\n");
  fprintf(stderr,
          "  -t, --tree      use a tree maze (default); this is easier\n");
  fprintf(stderr, "  -v, --version   display version and exit\n");
  fprintf(stderr, "  -w, --warranty  display warranty (none) and exit\n");
  fprintf(stderr,
          "Note that the use of xlaby on another display is discouraged\n");
  fprintf(stderr, "because reporting pointer motion events burns a lot of\n");
  fprintf(stderr,
          "bandwidth on slow networks. If you want to play a bad joke\n");
  fprintf(stderr,
          "on your best ennemy, it would be best to get him to start\n");
  fprintf(stderr,
          "the program on his own machine. I am _not_ recommending this.\n");
  printwarning();
  fprintf(stderr, "The source code for this program is available at:\n");
  fprintf(stderr, "  http://www.eleves.ens.fr:8080/home/madore/xlaby.tgz\n");
  fprintf(stderr, "Please send bug reports to david.madore@ens.fr\n");
}

void printinfo(void)
/* Print instructions */
{
  banner();
  fprintf(stderr,
          "The goal of the game is to eat the four little squares in the\n");
  fprintf(stderr,
          "labyrinth: red, yellow, green and blue, in that order. When\n");
  fprintf(stderr,
          "you eat a square, it disappears. When all have disappeared, the\n");
  fprintf(stderr,
          "game ends. The fun thing about this game is that it uses the\n");
  fprintf(stderr,
          "mouse pointer directly: the mouse is restricted to follow the\n");
  fprintf(stderr,
          "maze. To make things even more infuriating, the quit key (which\n");
  fprintf(stderr,
          "is normaly q or <Escape>) can be disabled with the --noquit\n");
  fprintf(stderr,
          "option. The keyboard can also be grabbed with the --grabkbd\n");
  fprintf(stderr,
          "option (this forbids an emergency call to the window manager).\n");
  printwarning();
  fprintf(stderr,
          "Other game options include: quantum mode (where the mouse will\n");
  fprintf(stderr,
          "occasionally make a quantum leap to a random position in the\n");
  fprintf(stderr,
          "maze), discovery mode (the walls become visible only when you\n");
  fprintf(stderr,
          "bump into them), and blind mode (you can guess what that means!)\n");
  fprintf(stderr,
          "The keys are: q or <Escape> to quit, z or <Tab> to suspend the\n");
  fprintf(stderr,
          "game and to resume it, and <Space> to force the window to be\n");
  fprintf(stderr, "visible (this should never be needed).\n");
}

void printwarranty(void)
/* Print (absence of) warranty */
{
  banner();
  fprintf(
      stderr,
      "(Please consider the following text to be in very large capitals.)\n");
  fprintf(
      stderr,
      "This program comes with absolutely no warranty of any kind (see the\n");
  fprintf(
      stderr,
      "GNU General Public License for details if you don't know what this\n");
  fprintf(stderr, "means; when I say no warrantee, I really mean _none_).\n");
  fprintf(
      stderr,
      "In particular, you run this program at your own risk, and any loss\n");
  fprintf(
      stderr,
      "of mental capabilities or precious computer data which may result\n");
  fprintf(stderr,
          "as a consequence of running this program is entirely your fault.\n");
}

void printcopying(void)
/* Print copying info */
{
  banner();
  fprintf(
      stderr,
      "This program is free software; you can redistribute it and/or modify\n");
  fprintf(
      stderr,
      "it under the terms of the GNU General Public License as published by\n");
  fprintf(
      stderr,
      "the Free Software Foundation; either version 2 of the License, or\n");
  fprintf(stderr, "(at your option) any later version.\n");
  fprintf(stderr, "\n");
  fprintf(stderr,
          "This program is distributed in the hope that it will be useful,\n");
  fprintf(stderr,
          "but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
  fprintf(stderr,
          "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
  fprintf(stderr, "GNU General Public License for more details.\n");
  fprintf(stderr, "\n");
  fprintf(
      stderr,
      "You should have received a copy of the GNU General Public License\n");
  fprintf(stderr,
          "along with this program; if not, write to the Free Software\n");
  fprintf(stderr, "Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA "
                  "02111-1307, USA\n");
}

void parseargs(int argc, char **argv)
/* Parse the command line */
{
  int i;
  rungame = 0;
  noquit = 0;
  grabkbd = 0;
  quantum = 0;
  visibility = 0;
  mazetype = 2;
  mazesize = 5;
  slowbuild = 0;
  display = NULL;
  wposx = -1;
  wposy = -1;
  for (i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--play") == 0)) {
      rungame = 1;
    } else if ((strcmp(argv[i], "--no-play") == 0) ||
               (strcmp(argv[i], "--noplay") == 0)) {
      rungame = 1;
      nogame = 1; /* Perhaps slightly contradictory... I should have chosen my
                     names better. */
    } else if ((strcmp(argv[i], "--no-quit") == 0) ||
               (strcmp(argv[i], "--noquit") == 0)) {
      noquit = 1;
    } else if ((strcmp(argv[i], "--grabkbd") == 0)) {
      grabkbd = 1;
    } else if ((strcmp(argv[i], "--build") == 0) ||
               (strcmp(argv[i], "--slowbuild") == 0)) {
      slowbuild = 1;
    } else if ((strcmp(argv[i], "-q") == 0) ||
               (strcmp(argv[i], "--quantum") == 0)) {
      quantum = 1;
    } else if ((strcmp(argv[i], "-d") == 0) ||
               (strcmp(argv[i], "--discover") == 0)) {
      visibility = 1;
    } else if ((strcmp(argv[i], "-b") == 0) ||
               (strcmp(argv[i], "--blind") == 0)) {
      visibility = 2;
    } else if ((strcmp(argv[i], "-t") == 0) ||
               (strcmp(argv[i], "--tree") == 0) ||
               (strcmp(argv[i], "--treemaze") == 0)) {
      mazetype = 0;
    } else if ((strcmp(argv[i], "-f") == 0) ||
               (strcmp(argv[i], "--flood") == 0) ||
               (strcmp(argv[i], "--floodmaze") == 0)) {
      mazetype = 1;
    } else if ((strcmp(argv[i], "-c") == 0) ||
               (strcmp(argv[i], "--chain") == 0) ||
               (strcmp(argv[i], "--chainmaze") == 0)) {
      mazetype = 2;
    } else if ((strcmp(argv[i], "-s") == 0) ||
               (strcmp(argv[i], "--size") == 0) ||
               (strcmp(argv[i], "--mazesize") == 0)) {
      if (++i >= argc) {
        fprintf(stderr, "Expected maze size at position %d.\n", i);
        exit(1);
      }
      if (sscanf(argv[i], "%d", &mazesize) == 0) {
        fprintf(stderr, "Error parsing maze size %s.\n", argv[i]);
        exit(1);
      }
      if (mazesize < 0)
        mazesize = 0;
      if (mazesize > 9)
        mazesize = 9;
    } else if ((strcmp(argv[i], "--pos") == 0) ||
               (strcmp(argv[i], "--position") == 0)) {
      if (++i >= argc) {
        fprintf(stderr, "Expected maze X position at position %d.\n", i);
        exit(1);
      }
      if (sscanf(argv[i], "%d", &wposx) == 0) {
        fprintf(stderr, "Error parsing window X position %s.\n", argv[i]);
        exit(1);
      }
      if (++i >= argc) {
        fprintf(stderr, "Expected maze Y position at position %d.\n", i);
        exit(1);
      }
      if (sscanf(argv[i], "%d", &wposy) == 0) {
        fprintf(stderr, "Error parsing window Y position %s.\n", argv[i]);
        exit(1);
      }
      /* We do not verify that wposx and wposy are valid:
       * that will be done in sizemaze() after the screen size is known. */
    } else if ((strcmp(argv[i], "-h") == 0) ||
               (strcmp(argv[i], "--help") == 0)) {
      printhelp(argv[0]);
      exit(0);
    } else if ((strcmp(argv[i], "-i") == 0) ||
               (strcmp(argv[i], "--info") == 0)) {
      printinfo();
      exit(0);
    } else if ((strcmp(argv[i], "-v") == 0) ||
               (strcmp(argv[i], "--version") == 0)) {
      banner();
      exit(0);
    } else if ((strcmp(argv[i], "-w") == 0) ||
               (strcmp(argv[i], "--warranty") == 0)) {
      printwarranty();
      exit(0);
    } else if ((strcmp(argv[i], "--copying") == 0)) {
      printcopying();
      exit(0);
    } else if ((strcmp(argv[i], "-display") == 0) ||
               (strcmp(argv[i], "--display") == 0)) {
      if (++i >= argc) {
        fprintf(stderr, "Expected display name at position %d.\n", i);
        exit(1);
      }
      display = argv[i];
    } else {
      fprintf(stderr, "Unknown option %s\n", argv[i]);
      exit(1);
    }
  }
  fprintf(stderr, "xlaby: type  %s --help  if you need help\n", argv[0]);
}

void sizemaze(void)
/* Calculate size parameters */
{
  cellwidth = 9;
  cellheight = 9;
  switch (mazesize) {
  case 0:
    cellhoriz = 8;
    cellvert = 8;
    cellwidth = 12;
    cellheight = 12;
    break;
  case 1:
    cellhoriz = 15;
    cellvert = 15;
    cellwidth = 10;
    cellheight = 10;
    break;
  case 2:
    cellhoriz = 30;
    cellvert = 30;
    break;
  case 3:
    cellhoriz = 45;
    cellvert = 45;
    break;
  case 4:
    cellhoriz = 60;
    cellvert = 60;
    break;
  case 5:
    cellhoriz = 85;
    cellvert = 65;
    break;
  case 6:
    cellhoriz = 100;
    cellvert = 80;
    break;
  case 7:
    cellhoriz = 110;
    cellvert = 90;
    break;
  case 8:
    cellhoriz = 120;
    cellvert = 100;
    cellwidth = 8;
    cellheight = 8;
    break;
  case 9:
    cellhoriz = 150;
    cellvert = 120;
    cellwidth = 7;
    cellheight = 7;
    break;
  }
  while (cellhoriz > MAXHORIZ)
    cellhoriz--;
  while ((cellwidth >= 5) && (screenwidth < MINSCREENWIDTH))
    cellwidth--;
  while ((cellhoriz >= 5) && (screenwidth < MINSCREENWIDTH))
    cellhoriz--;
  if (screenwidth <
      MINSCREENWIDTH) { /* Screen resolution is less than 40x40 or so!*/
    fprintf(stderr, "That screen is too small for me. Sorry.\n");
    exit(1);
  }
  while (cellvert > MAXVERT)
    cellvert--;
  while ((cellheight >= 5) && (screenheight < MINSCREENHEIGHT))
    cellheight--;
  while ((cellvert >= 5) && (screenheight < MINSCREENHEIGHT))
    cellvert--;
  if (screenheight <
      MINSCREENHEIGHT) { /* Screen resolution is less than 40x40 or so!*/
    fprintf(stderr, "That screen is too small for me. Sorry.\n");
    exit(1);
  }
  if ((wposx < 0) || (wposy < 0) || (wposx >= screenwidth - WINWIDTH) ||
      (wposy >= screenheight - WINHEIGHT)) {
    wposx = random() % (screenwidth - WINWIDTH);
    wposy = random() % (screenheight - WINHEIGHT);
  }
}

void getthecolor(XColor *thecolor)
/* Takes an XColor and sets its pixel value to something in the colormap which
 * matches it as closely as possible. This is done with XAllocColor; if the
 * colormap is full, we use our own. */
{
  while (!XAllocColor(dpy, cmap, thecolor)) {
    if (!owncmap) {
      cmap = XCopyColormapAndFree(dpy, cmap);
      XSetWindowColormap(dpy, myw, cmap);
      owncmap = 1;
    } else {
      fprintf(stderr, "Hmmm... This screen appears to be color starved...\n");
      exit(1);
    }
  }
}

void xgetcolors(void)
/* Sets up the colors. Since the Xlib manual is extremely unclear in that
 * domain, this is somewhat experimental... */
{
  XWindowAttributes watt;
  XColor c_garbage;
  XGetWindowAttributes(dpy, myw, &watt);
  cmap = watt.colormap;
  owncmap = 0;
  if (!XLookupColor(dpy, cmap, "Gray70", &c_garbage, &c_back)) {
    c_back.red = 52428;
    c_back.green = 52428;
    c_back.blue = 52428;
  }
  getthecolor(&c_back);
  if (!XLookupColor(dpy, cmap, "Gray10", &c_garbage, &c_wall)) {
    c_wall.red = 6554;
    c_wall.green = 6554;
    c_wall.blue = 6554;
  }
  getthecolor(&c_wall);
  if (!XLookupColor(dpy, cmap, "White", &c_garbage, &c_goal[0])) {
    c_goal[0].red = 65535;
    c_goal[0].green = 65535;
    c_goal[0].blue = 65535;
  }
  getthecolor(&c_goal[0]);
  if (!XLookupColor(dpy, cmap, "Red", &c_garbage, &c_goal[1])) {
    c_goal[1].red = 65535;
    c_goal[1].green = 0;
    c_goal[1].blue = 0;
  }
  getthecolor(&c_goal[1]);
  if (!XLookupColor(dpy, cmap, "Yellow", &c_garbage, &c_goal[2])) {
    c_goal[2].red = 65535;
    c_goal[2].green = 65535;
    c_goal[2].blue = 0;
  }
  getthecolor(&c_goal[2]);
  if (!XLookupColor(dpy, cmap, "Green", &c_garbage, &c_goal[3])) {
    c_goal[3].red = 0;
    c_goal[3].green = 65535;
    c_goal[3].blue = 0;
  }
  getthecolor(&c_goal[3]);
  if (!XLookupColor(dpy, cmap, "Blue", &c_garbage, &c_goal[4])) {
    c_goal[4].red = 0;
    c_goal[4].green = 0;
    c_goal[4].blue = 65535;
  }
  getthecolor(&c_goal[4]);
}

void xprepare0(void)
/* Preliminary setting up of X11 */
{
  if ((dpy = XOpenDisplay(display)) == NULL) {
    fprintf(stderr, "Couldn't open display %s\n", display);
    exit(1);
  }
  scn = DefaultScreen(dpy);
  screen = ScreenOfDisplay(dpy, scn);
  screenwidth = WidthOfScreen(screen);
  screenheight = HeightOfScreen(screen);
  root = RootWindow(dpy, scn);
}

void xprepare1(void)
/* This does the X setting up which couldn't be done before the
 * maze size was calculated */
{
  myw = XCreateSimpleWindow(dpy, root, 0, 0, WINWIDTH, WINHEIGHT, 5,
                            WhitePixel(dpy, scn), BlackPixel(dpy, scn));
  XSelectInput(dpy, myw,
               KeyPressMask | KeyReleaseMask | ExposureMask | EnterWindowMask |
                   VisibilityChangeMask);
  XMapRaised(dpy, myw);
  subw = XCreateWindow(dpy, myw, 0, 0, cellwidth, cellheight, 0, CopyFromParent,
                       InputOnly, CopyFromParent, 0, NULL);
  /* The Xlib manual is unclear about whether the following line matters when we
   * are grabbing the pointer. I guess not, but I don't care for trying. */
  XSelectInput(dpy, subw, PointerMotionMask);
  XMapRaised(dpy, subw);
  gc = XCreateGC(dpy, myw, 0, NULL);
  XSync(dpy, False);
  XChangeProperty(dpy, myw, XA_WM_NAME, XA_STRING, 8, PropModeReplace,
                  (char *)wname, strlen(wname));
  xgetcolors();
  XSync(dpy, False);
  pmexists = 0;
  grabbing = 0;
  curs = XCreateFontCursor(
      dpy, XC_top_left_arrow); /* This appears to give the best results */
}

void makevisible(void)
/* Make sure the window is completely visible. */
{
  /* Unfortunately, there is no way to do that... So we'll do the best we can.
   */
  XResizeWindow(dpy, myw, WINWIDTH,
                WINHEIGHT);            /* Dear, I almost forgot that! */
  XMoveWindow(dpy, myw, wposx, wposy); /* Yuck... How ugly... */
  XRaiseWindow(dpy, myw); /* Make sure the window is really on top... */
  XSync(dpy, False);      /* This seems needed (I can't see why) */
}

void grabbit(void)
/* This actually grabs the pointer (and keyboard). */
{
  grabbing = 1;
  /* The following commands should make _sure_ the window and game are in
   * a playable situation */
  makevisible();
  XMoveWindow(dpy, subw, subx * cellwidth, suby * cellheight);
  XSync(dpy, False); /* This seems needed (I can't see why) */
  while (XGrabPointer(dpy, subw, False, PointerMotionMask, GrabModeAsync,
                      GrabModeAsync, subw, curs, CurrentTime) != GrabSuccess)
    ; /* For some reason, we need a loop here */
  /* It may screw up the machine, though. I should add a timeout. */
  XRaiseWindow(dpy, myw); /* "Deux précautions valent mieux qu'une" */
  if (grabkbd) {
    sleep(1);
    XSync(dpy, False);
    if (XGrabKeyboard(dpy, myw, False, GrabModeAsync, GrabModeAsync,
                      CurrentTime) != GrabSuccess) {
      fprintf(stderr, "Could not grab keyboard!\n");
    }
  }
  XWarpPointer(dpy, None, subw, 0, 0, 0, 0, cellwidth / 2, cellheight / 2);
}

void ungrabbit(void) {
  grabbing = 0;
  XUngrabPointer(dpy, CurrentTime);
  XUngrabKeyboard(dpy, CurrentTime);
}

void youwon(void)
/* Prints a congratulation message and quits */
{
  fprintf(stdout, "Congratulations! You won!\n");
  /* A bit dry, perhaps... */
  quit(42);
}

void mainloop(void)
/* This is the main program loop. Note that this function never returns.
 * It handles events received for both windows. */
{
  XEvent evt;
  KeySym key;
  int dx, dy;
  if (!nogame)
    grabbit();
  while (1) {
    XNextEvent(dpy, &evt);
    switch (evt.type) {
    case Expose:
      if ((evt.xexpose.x < WINWIDTH) && (evt.xexpose.y < WINHEIGHT)) {
        refresh(evt.xexpose.x, evt.xexpose.y,
                MIN(evt.xexpose.width, WINWIDTH - evt.xexpose.x),
                MIN(evt.xexpose.height, WINHEIGHT - evt.xexpose.y));
      }
      break;
    case VisibilityNotify:
      /* Insist on staying on top while grabbing the pointer.
       * Otherwise game might become unplayable! */
      if ((grabbing) &&
          ((evt.xvisibility.state == VisibilityPartiallyObscured) ||
           (evt.xvisibility.state == VisibilityFullyObscured)))
        makevisible();
      break;
    case EnterNotify:
#if 0 /* Used to grab the pointer when it entered the window (v.1.5.0),        \
       * but that is infuriating */
      if ((!nogame)&&(evt.xcrossing.detail!=NotifyInferior)) grabbit();
#endif
      break;
    case KeyPress:
      key = XKeycodeToKeysym(dpy, evt.xkey.keycode, evt.xkey.state);
      if (((key == XK_Escape) || (key == XK_Q) || (key == XK_q)) && (!noquit))
        quit(0);
      if (((key == XK_Tab) || (key == XK_Z) || (key == XK_z)) &&
          (!noquit)) { /* Why Z? Dunno. */
        if (grabbing)
          ungrabbit();
        else if (!nogame)
          grabbit();
      } else if (key == XK_space)
        makevisible();
      break;
    case KeyRelease:
      /* I don't use that. But I might, some day */
      break;
    case MotionNotify:
      if (!grabbing)
        break; /* Don't move subwindow while not playing game */
      if (evt.xmotion.x == 0)
        dx = -1;
      else if (evt.xmotion.x == cellwidth - 1)
        dx = 1;
      else
        dx = 0;
      if (evt.xmotion.y == 0)
        dy = -1;
      else if (evt.xmotion.y == cellheight - 1)
        dy = 1;
      else
        dy = 0;
      if ((!dx) && (!dy))
        break;
      if (subx + dx < 0)
        dx = 0;
      if (suby + dy < 0)
        dy = 0;
      if (subx + dx >= cellhoriz)
        dx = 0;
      if (suby + dy >= cellvert)
        dy = 0;
      if ((dx == -1) && (wallv[subx][suby])) {
        dx = 0;
        wallv[subx][suby] = 2;
      }
      if ((dy == -1) && (wallh[subx][suby])) {
        dy = 0;
        wallh[subx][suby] = 2;
      }
      if ((dx == 1) && (wallv[subx + 1][suby])) {
        dx = 0;
        wallv[subx + 1][suby] = 2;
      }
      if ((dy == 1) && (wallh[subx][suby + 1])) {
        dy = 0;
        wallh[subx][suby + 1] = 2;
      }
      /*
       * The following lines are used to avoid the case where the pointer jumps
       * across a corner "seen from the outside". I would like something
       * cleaner, but I didn't find it.
       */
      if ((dx == 1) && (dy == 1) && (wallh[subx + 1][suby + 1]) &&
          (wallv[subx + 1][suby + 1])) {
        dx = 0;
        dy = 0;
      }
      if ((dx == -1) && (dy == 1) && (wallh[subx - 1][suby + 1]) &&
          (wallv[subx][suby + 1])) {
        dx = 0;
        dy = 0;
      }
      if ((dx == 1) && (dy == -1) && (wallh[subx + 1][suby]) &&
          (wallv[subx + 1][suby - 1])) {
        dx = 0;
        dy = 0;
      }
      if ((dx == -1) && (dy == -1) && (wallh[subx - 1][suby]) &&
          (wallv[subx][suby - 1])) {
        dx = 0;
        dy = 0;
      }
      if (visibility == 1) { /* In "discover" mode, we need to refresh the area
                                around the mouse */
        redraw(subx - 1, suby - 1, 3, 3);
        refresh((subx - 1) * cellwidth, (suby - 1) * cellwidth, 3 * cellwidth,
                3 * cellheight);
      }
      if ((!dx) && (!dy))
        break;
      if (quantum && ((random() % 1000000) <= QPROB)) {
        subx = random() % cellhoriz; /* Do a quantum leap */
        suby = random() % cellvert;
      } else
        subx += dx;
      suby += dy;
      XMoveWindow(dpy, subw, subx * cellwidth, suby * cellheight);
      if ((subx == goals[reached + 1].x) && (suby == goals[reached + 1].y)) {
        reached++;
        redraw(subx, suby, 1, 1);
        refresh(subx * cellwidth, suby * cellwidth, cellwidth, cellheight);
      }
      if (reached == MAZEGOALS - 1)
        youwon();
      break;
    }
  }
}

void main(int argc, char **argv)
/* Main program */
{
  srandom(time(NULL));
  parseargs(argc, argv);
  if (!rungame)
    exit(0);
  xprepare0();
  sizemaze();
  xprepare1();
  maze();
  redraw(0, 0, cellhoriz, cellvert);
  mainloop();
}

/* That's all, folks! */
