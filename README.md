# Graphic Era Hill University Bus Tracker

Website (GitHub Pages) plus C and C++ programs.

- `index.html` the whole website (no build step). Buses are simulated by default.
- `CNAME` custom domain: gehubustracker.com
- `404.html` sends unknown paths back to the home page
- `server/bus_server.cpp` C++ web server (min-heap, hash table, Dijkstra, merge sort, binary search)
- `server/bus_tracker.cpp` and `server/bus_tracker.c` terminal versions of the same logic

GitHub Pages cannot run the C/C++ files. They are for viewing and downloading.
To use the server, run it and set `API_URL` near the top of the script in `index.html`.

Build:
- `g++ -std=c++17 -o bus_server server/bus_server.cpp` (Windows: add `-lws2_32`)
- `g++ -std=c++17 -o bus_tracker server/bus_tracker.cpp`
- `gcc -std=c99 -o bus_tracker server/bus_tracker.c`
