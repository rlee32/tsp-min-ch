#include "nano_timer.hh"
#include "config.hh"
#include "fileio.hh"
#include "point_quadtree/point_quadtree.hh"
#include "box_maker.hh"
#include "length_calculator.hh"

#include <random>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

int main(int argc, const char** argv) {
    if (argc == 1) {
        std::cout << "Arguments: config_file_path" << std::endl;
    }
    // Read config file.
    const std::string config_path = (argc == 1) ? "config.txt" : argv[1];
    std::cout << "Reading config file: " << config_path << std::endl;
    Config config(config_path);

    // Read input files.
    const std::optional<std::string> tsp_file_path_string = config.get("tsp_file_path");
    if (not tsp_file_path_string) {
        std::cout << "tsp_file_path not specified.\n";
        return EXIT_FAILURE;
    }
    const std::optional<std::filesystem::path> tsp_file_path(*tsp_file_path_string);
    const auto [x, y] = fileio::read_coordinates(*tsp_file_path_string);

    point_quadtree::Domain domain(x, y);
    std::cout << "domain aspect ratio: " << domain.xdim(0) / domain.ydim(0) << std::endl;
    std::cout << "bounding x, y dim: "
        << domain.xdim(0) << ", " << domain.ydim(0)
        << std::endl;

    // Quad tree.
    NanoTimer timer;
    timer.start();

    std::cout << "\nquadtree stats:\n";
    const auto root {point_quadtree::make_quadtree(x, y, domain)};
    std::cout << "node ratio: "
        << static_cast<double>(point_quadtree::count_nodes(root))
            / point_quadtree::count_points(root)
        << std::endl;
    std::cout << "Finished quadtree in " << timer.stop() / 1e9 << " seconds.\n\n";

    // Tracks endpoints for every segment so that we can avoid making a cycle before a complete tour can be formed.
    std::unordered_map<primitives::point_id_t, primitives::point_id_t> endpoints;

    const auto n = x.size();

    // Remaining points with degree < 2. Note that this is not actual point ID, but sequence ID in the random_mapping.
    std::unordered_set<primitives::sequence_t> remaining;
    for (primitives::sequence_t i{0}; i < n; ++i) {
        remaining.insert(i);
    }

    // Shuffle points to process in random order to get a different tour every time.
    // These random mappings and their usage throughout add about 25% to overall runtime for large instance (lrb744710).
    std::random_device rd;
    std::mt19937 g(rd());

    std::vector<primitives::point_id_t> random_mapping;
    random_mapping.reserve(n);
    for (primitives::point_id_t i{0}; i < n; ++i) {
        random_mapping[i] = i;
    }
    std::shuffle(std::begin(random_mapping), std::end(random_mapping), g);

    std::vector<primitives::point_id_t> sequence_mapping;
    sequence_mapping.reserve(n);
    for (primitives::point_id_t i{0}; i < n; ++i) {
        sequence_mapping[random_mapping[i]] = i;
    }

    // Tracks degree (number of edges attached) of each point.
    std::vector<size_t> degree(n, 0);

    // Adjacency map that will be used to construct the final tour.
    std::unordered_map<primitives::point_id_t, std::vector<primitives::point_id_t>> adjacency;

    // go through all points and generate edges.
    BoxMaker box_maker(x, y);
    size_t total_points{0};
    double radius = 1;
    size_t finished_count{0};
    while (remaining.size() > 2) {
        std::unordered_set<primitives::point_id_t> finished;
        for (const auto &s : remaining) {
            auto i = random_mapping[s];
            if (degree[i] == 2) {
                continue;
            }
            auto points = root.get_points(i, box_maker(i, radius));
            std::shuffle(std::begin(points), std::end(points), g);
            for (const auto &p : points) {
                if (degree[p] == 2 or p == i) {
                    continue;
                }
                if (adjacency.find(i) != std::cend(adjacency) and adjacency[i].size() == 1) {
                    if (adjacency[i][0] == p) {
                        continue;
                    }
                }
                if (adjacency.find(p) != std::cend(adjacency) and adjacency[p].size() == 1) {
                    if (adjacency[p][0] == i) {
                        continue;
                    }
                }
                if (degree[p] == 0 and degree[i] == 0) {
                    assert(endpoints.find(i) == std::cend(endpoints));
                    assert(endpoints.find(p) == std::cend(endpoints));
                    endpoints[i] = p;
                    endpoints[p] = i;
                    assert(adjacency.find(i) == std::cend(adjacency));
                    assert(adjacency.find(p) == std::cend(adjacency));
                } else if (degree[p] == 1 and degree[i] == 0) {
                    assert(endpoints.find(i) == std::cend(endpoints));
                    assert(endpoints.find(p) != std::cend(endpoints));
                    auto j = endpoints[p];
                    assert(endpoints.find(j) != std::cend(endpoints));
                    endpoints[i] = j;
                    endpoints[j] = i;
                    assert(adjacency.find(i) == std::cend(adjacency));
                    assert(adjacency.find(p) != std::cend(adjacency));
                } else if (degree[p] == 0 and degree[i] == 1) {
                    assert(endpoints.find(i) != std::cend(endpoints));
                    assert(endpoints.find(p) == std::cend(endpoints));
                    auto j = endpoints[i];
                    assert(endpoints.find(j) != std::cend(endpoints));
                    endpoints[p] = j;
                    endpoints[j] = p;
                    assert(adjacency.find(i) != std::cend(adjacency));
                    assert(adjacency.find(p) == std::cend(adjacency));
                } else {
                    assert(endpoints.find(i) != std::cend(endpoints));
                    assert(endpoints.find(p) != std::cend(endpoints));
                    auto j = endpoints[i];
                    if (j == p) {
                        continue;
                    }
                    assert(endpoints.find(j) != std::cend(endpoints));
                    auto k = endpoints[p];
                    assert(k != i);
                    assert(endpoints.find(k) != std::cend(endpoints));
                    endpoints[k] = j;
                    endpoints[j] = k;
                    assert(adjacency.find(i) != std::cend(adjacency));
                    assert(adjacency.find(p) != std::cend(adjacency));
                }
                adjacency[i].push_back(p);
                adjacency[p].push_back(i);
                for (const auto &j : {i, p}) {
                    ++degree[j];
                    assert(degree[j] < 3);
                    if (degree[j] == 2) {
                        assert(finished.find(j) == std::cend(finished));
                        finished.insert(j);
                    }
                }
                break;
            }
            total_points += points.size();
        }
        finished_count += finished.size();
        for (const auto &i : finished) {
            remaining.erase(sequence_mapping[i]);
        }
        radius *= 2;
    }
    assert(remaining.size() == 2);
    assert(finished_count == n - 2);

    // Note that this number can be reduced by removing finished points from the quadtree.
    std::cout << "total points obtained from quadtree: " << total_points << std::endl;

    // close up final edge to make a cycle.
    auto i = random_mapping[*std::cbegin(remaining)];
    auto j = random_mapping[*std::next(std::cbegin(remaining))];
    adjacency[i].push_back(j);
    assert(adjacency[i].size() == 2);
    adjacency[j].push_back(i);
    assert(adjacency[j].size() == 2);

    // check adjacency map.
    for (const auto &pair : adjacency) {
        assert(pair.second.size() == 2);
        assert(pair.second[0] != pair.second[1]);
    }

    // Generate tour.
    std::vector<primitives::point_id_t> tour;
    primitives::point_id_t prev{0};
    tour.push_back(prev);
    tour.push_back(adjacency[prev][0]);
    std::unordered_set<primitives::point_id_t> seen;
    seen.insert(prev);
    seen.insert(adjacency[prev][0]);
    while (tour.size() < n) {
        auto current = tour.back();
        const auto &adj = adjacency[current];
        if (prev == adj[0]) {
            tour.push_back(adj[1]);
        } else {
            assert(prev == adj[1]);
            tour.push_back(adj[0]);
        }
        seen.insert(tour.back());
        prev = current;
    }
    assert(seen.size() == n);

    prev = tour.back();
    LengthCalculator calc(x, y);
    primitives::length_t total_length{0};
    for (const auto &i : tour) {
        total_length += calc(prev, i);
        prev = i;
    }
    std::cout << "total length: " << total_length << std::endl;

    // Write new tour if output path specified.
    const auto &output_path = config.get("output_path");
    if (output_path) {
        fileio::write_ordered_points(tour, *output_path);
        std::cout << "saved new tour to " << *output_path << std::endl;
    } else {
        std::cout << "no output_path specified; not writing result to file." << std::endl;
    }

    return EXIT_SUCCESS;
}
