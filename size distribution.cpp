#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <omp.h>
#include <map>
#include <thread>

// Selects 'count' unique cluster indices at random
std::vector<int> select_random_clusters(const std::vector<int>& cluster_sizes, int count, std::mt19937& gen) {
    std::vector<int> indices;
    for (int i = 0; i < cluster_sizes.size(); ++i) {
    indices.push_back(i);          // every entry is active
}
    if (indices.size() < count) return {};
    std::shuffle(indices.begin(), indices.end(), gen);
    return std::vector<int>(indices.begin(), indices.begin() + count);
}

bool merge_clusters(std::vector<int>& cluster_sizes,double add_probability,std::mt19937& gen){

    // 1 · Select four distinct live clusters
    auto sel = select_random_clusters(cluster_sizes, 4, gen);
    if (sel.size() < 4) return false;

    int c1a = sel[0], c1b = sel[1];
    int c2a = sel[2], c2b = sel[3];

    int product1 = cluster_sizes[c1a] * cluster_sizes[c1b];
    int product2 = cluster_sizes[c2a] * cluster_sizes[c2b];

    int merge_a = (product1 < product2) ? c1a : c2a;   // slot that survives
    int merge_b = (product1 < product2) ? c1b : c2b;   // slot to remove

    // 2 · Create the merged cluster (sum of the two sizes)
    cluster_sizes[merge_a] += cluster_sizes[merge_b];
    int new_cluster_size = cluster_sizes[merge_a];      // remember its size

    // 3 · Erase merge_b via swap‑with‑back + pop_back()
    if (merge_b != static_cast<int>(cluster_sizes.size()) - 1) {
        std::swap(cluster_sizes[merge_b], cluster_sizes.back());
    }
    cluster_sizes.pop_back();                          // vector now dense

    // 4 · Probabilistically add a *clone* of the merged cluster
    std::uniform_real_distribution<> dis(0.0, 1.0);
    if (dis(gen) < add_probability) {
        cluster_sizes.push_back(new_cluster_size);     // ← same size as merge
    }
    return true;
}


void simulate(int N, int iterations, double add_probability,
             int target_iteration, std::map<double, int>& size_counts, std::mt19937& gen)
{
    std::vector<int> cluster_sizes(N, 1); // N clusters, each size 1

    for (int i = 0; i <= iterations; ++i) {
        int active_clusters = std::count_if(cluster_sizes.begin(), cluster_sizes.end(), [](int sz){ return sz > 0; });
        if (active_clusters < 4) break;

        /*if (i == target_iteration) {
    // Count cluster sizes in ranges (1-2, 3-4, 5-6, etc.)
    for (int sz : cluster_sizes) {

            // Determine the range (1-2, 3-4, 5-6, etc.)
            int range_start = ((sz - 1) / 2) * 2 + 1;
            double midpoint = range_start + 0.5; // Midpoint of the range (e.g., 1.5 for 1-2)

            // Increment count for this midpoint
            size_counts[midpoint]++;

    }
    break;
        }*/

        if ( i == target_iteration) {
            // Count cluster sizes in ranges
           for (int sz : cluster_sizes) {


                    // Determine the range (1-3, 4-6, 7-9, etc.)
                    int range_start = ((sz - 1) / 3) * 3 + 1;
                    int range_end = range_start + 1;
                    int midpoint = (range_start + range_end) / 2;
                    size_counts[midpoint]++;

            }
            break;
        }

      bool valid = merge_clusters(cluster_sizes, add_probability, gen);

        if (!valid) break;
    }
}



int main() {
    int N, iterations, runs = 1000, target_iteration;
    double add_probability;
    std::cout << "Enter initial number of clusters (N): ";
    std::cin >> N;
    std::cout << "Enter number of iterations: ";
    std::cin >> iterations;
    std::cout << "Enter probability of adding a new cluster (0.0 to 1.0): ";
    std::cin >> add_probability;
    std::cout << "Enter target iteration for size distribution: ";
    std::cin >> target_iteration;

    if (target_iteration > iterations) {
        std::cerr << "Error: Target iteration cannot be greater than total iterations\n";
        return 1;
    }

    // Calculate number of threads to use (80% of available cores)
    int max_threads = std::thread::hardware_concurrency();
    int use_threads = std::max(1, static_cast<int>(max_threads * 0.7));
    omp_set_num_threads(use_threads);  // Limit OpenMP to 80% of cores

    std::cout << "Using " << use_threads << " threads out of " << max_threads
              << " available cores (80% utilization)\n";

    std::map<double, int> total_size_counts;

    #pragma omp parallel
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::map<double, int> local_size_counts;

        #pragma omp for
        for (int r = 0; r < runs; ++r) {
            std::map<double, int> run_size_counts;
            simulate(N, iterations, add_probability, target_iteration, run_size_counts, gen);

            for (const auto& pair : run_size_counts) {
                local_size_counts[pair.first] += pair.second;
            }
        }

        #pragma omp critical
        {
            for (const auto& pair : local_size_counts) {
                total_size_counts[pair.first] += pair.second;
            }
        }
    }

    // Prepare data for output
    std::vector<std::pair<double, double>> results;
    for (const auto& pair : total_size_counts) {
        results.emplace_back(pair.first, static_cast<double>(pair.second) / runs);
    }

    // Sort by cluster size midpoint
    std::sort(results.begin(), results.end());

    std::ofstream outfile("sizep=0.txt");
    outfile << "ClusterSizeMidpoint NumberOfClusters\n";
    for (const auto& [midpoint, count] : results) {
        outfile << midpoint << " " << count << "\n";
    }
    outfile.close();

    std::cout << "Simulation complete. Results saved to cluster_size_distribution.txt\n";
    return 0;
}
