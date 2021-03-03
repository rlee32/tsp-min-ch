A construction heuristic that tries to add smallest edges first.
Similar to nearest neighbor, but edges are not restricted to be added at a single point at a time.

Utilizes a quadtree for efficiency. Approximately O(n * log(n)) work complexity.

Edges are added in iterations, with each iteration using a fixed search radius to join 2 points together.
Only 0-degree points (points not yet with an edge) can be joined to 1-degree points (points with 1 edge). In this way, a single tour can easily be made.

