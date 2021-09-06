A construction heuristic that tries to add smallest edges first.
Similar to nearest neighbor, but edges are not restricted to be added at a single point at a time.

Edges are added in iterations, with each iteration using a fixed search radius to join 2 points together.
Segment endpoints are tracked so that premature cycles are avoided.

Utilizes a quadtree for efficiency. Overall, approximately O(n * log(n)) work complexity, with resulting tour about 30% above optimum.

Sample measurements from my macbook air laptop:
- xrb14233: 0.1 s
- bby34656: 0.27 s
- lrb744710: 6 s

TODO
1. Implement ability to delete points from quadtree.
