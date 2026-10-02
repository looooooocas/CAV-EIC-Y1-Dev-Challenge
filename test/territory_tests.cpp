#include "antworld.h"
#include "utilities.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {
int failures = 0;
int checks = 0;

void check(bool condition, const std::string& description) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << description << '\n';
    }
}

void testAngleAndWithin() {
    MapTemplate map(15, std::vector<int>(15, 0));
    Coord home(7, 7);

    // 4 scouts -> 4 quadrants of size PI/2
    Territory t0(home, 0, 4, map); // [-PI, -PI/2)
    Territory t1(home, 1, 4, map); // [-PI/2, 0)
    Territory t2(home, 2, 4, map); // [0, PI/2)
    Territory t3(home, 3, 4, map); // [PI/2, PI)

    // In Territory::angle, atan2(col - home.col, row - home.row):
    // South (+row) is angle 0
    // East (+col) is angle PI/2
    // North (-row) is angle +-PI
    // West (-col) is angle -PI/2
    check(std::abs(t0.angle(Coord(12, 7)) - 0.0) < 1e-4, "South has angle 0");
    check(std::abs(t0.angle(Coord(7, 12)) - (PI / 2)) < 1e-4, "East has angle PI/2");
    check(std::abs(std::abs(t0.angle(Coord(2, 7))) - PI) < 1e-4, "North has angle +-PI");
    check(std::abs(t0.angle(Coord(7, 2)) - (-PI / 2)) < 1e-4, "West has angle -PI/2");

    // Quadrant points relative to home (7, 7):
    Coord northWest(4, 4);  // delta row = -3, delta col = -3 -> angle -3*PI/4 in [-PI, -PI/2] -> t0
    Coord southWest(10, 4); // delta row = +3, delta col = -3 -> angle -PI/4   in [-PI/2, 0]  -> t1
    Coord southEast(10, 10);// delta row = +3, delta col = +3 -> angle +PI/4   in [0, PI/2]   -> t2
    Coord northEast(4, 10); // delta row = -3, delta col = +3 -> angle +3*PI/4 in [PI/2, PI]  -> t3

    check(t0.within(northWest), "northWest is within t0");
    check(t1.within(southWest), "southWest is within t1");
    check(t2.within(southEast), "southEast is within t2");
    check(t3.within(northEast), "northEast is within t3");
}

void testStartHeading() {
    MapTemplate map(15, std::vector<int>(15, 0));
    int rows = 15, cols = 15;
    int foodRadius = 2;

    std::vector<std::pair<Coord, std::string>> homePositions = {
        {{7, 7}, "Center (7, 7)"},
        {{0, 7}, "Top edge (0, 7)"},
        {{14, 7}, "Bottom edge (14, 7)"},
        {{7, 0}, "Left edge (7, 0)"},
        {{7, 14}, "Right edge (7, 14)"},
        {{0, 0}, "Top-Left corner (0, 0)"},
        {{14, 14}, "Bottom-Right corner (14, 14)"}
    };

    for (const auto& [home, desc] : homePositions) {
        std::cout << "\n=== " << desc << " ===\n";
        for (int numScouts = 1; numScouts <= 8; ++numScouts) {
            std::cout << "  numScouts=" << numScouts << ":\n";
            for (int i = 0; i < numScouts; ++i) {
                Territory t(home, i, numScouts, map);
                Coord sp = t.startPoint(foodRadius);
                Coord heading = t.startHeading(foodRadius);

                check(heading != Coord(0, 0),
                      desc + ": Scout " + std::to_string(i) + " startHeading != (0,0)");

                // Heading is non-zero and roughly scaled by foodRadius
                double hLen = std::hypot(heading.first, heading.second);
                check(hLen > 0.0 && hLen <= foodRadius * 1.5,
                      desc + ": Scout " + std::to_string(i) + " heading magnitude is reasonable (" +
                      std::to_string(heading.first) + "," + std::to_string(heading.second) + ")");

                // In bounds check for one step along heading
                int nr = sp.first + heading.first;
                int nc = sp.second + heading.second;
                check(nr >= 0 && nr < rows && nc >= 0 && nc < cols,
                      desc + ": Scout " + std::to_string(i) + " step along heading is in bounds");

                std::cout << "    Scout " << i << ": startPoint=(" << sp.first << "," << sp.second
                          << "), heading=(" << heading.first << "," << heading.second
                          << "), next=(" << nr << "," << nc << ")\n";
            }
        }
    }
}

void testNextVantageProgression() {
    MapTemplate map(15, std::vector<int>(15, 0));
    Coord home(7, 7);
    int foodRadius = 2;

    for (int numScouts = 1; numScouts <= 8; ++numScouts) {
        std::cout << "\n--- Coverage for numScouts=" << numScouts << " (home at 7,7) ---\n";
        for (int i = 0; i < numScouts; ++i) {
            Territory t(home, i, numScouts, map);
            Coord vantage = t.startPoint(foodRadius);
            Coord heading = t.startHeading(foodRadius);

            // Collect all cells in this scout's territory
            std::vector<Coord> territoryCells;
            for (int r = 0; r < 15; ++r) {
                for (int c = 0; c < 15; ++c) {
                    if (r == home.first && c == home.second) continue;
                    if (t.within(Coord(r, c))) {
                        territoryCells.push_back(Coord(r, c));
                    }
                }
            }

            // Track seen cells (within foodRadius of any vantage)
            std::vector<std::vector<bool>> seen(15, std::vector<bool>(15, false));
            auto markSeen = [&](Coord v) {
                for (int dr = -foodRadius; dr <= foodRadius; ++dr) {
                    for (int dc = -foodRadius; dc <= foodRadius; ++dc) {
                        int r = v.first + dr;
                        int c = v.second + dc;
                        if (r >= 0 && r < 15 && c >= 0 && c < 15) {
                            seen[r][c] = true;
                        }
                    }
                }
            };

            markSeen(vantage);

            int stuckAtOriginCount = 0;
            int sameVantageCount = 0;

            for (int step = 0; step < 120; ++step) {
                Coord next = t.nextVantage(vantage, heading);

                check(next.first >= 0 && next.first < 15,
                      "nextVantage row in bounds for scout " + std::to_string(i) + " at step " + std::to_string(step));
                check(next.second >= 0 && next.second < 15,
                      "nextVantage col in bounds for scout " + std::to_string(i) + " at step " + std::to_string(step));

                if (next == Coord(0, 0)) {
                    stuckAtOriginCount++;
                } else {
                    stuckAtOriginCount = 0;
                }
                check(stuckAtOriginCount < 3,
                      "Scout " + std::to_string(i) + " got stuck at (0, 0) (numScouts=" + std::to_string(numScouts) + ")");

                markSeen(next);
                vantage = next;
            }

            int seenCount = 0;
            std::vector<Coord> unseen;
            for (const auto& c : territoryCells) {
                if (seen[c.first][c.second]) {
                    seenCount++;
                } else {
                    unseen.push_back(c);
                }
            }

            double pct = 100.0 * seenCount / territoryCells.size();
            std::cout << "  Scout " << i << ": covered " << seenCount << "/" << territoryCells.size()
                      << " cells (" << pct << "%)\n";

            if (!unseen.empty()) {
                std::cout << "    Unseen (" << unseen.size() << " cells): ";
                for (size_t u = 0; u < std::min(unseen.size(), size_t(8)); ++u) {
                    std::cout << "(" << unseen[u].first << "," << unseen[u].second << ") ";
                }
                if (unseen.size() > 8) std::cout << "...";
                std::cout << "\n";
            }
        }
    }
}

void testCornerVantageBehavior() {
    MapTemplate map(15, std::vector<int>(15, 0));
    Coord home(7, 7);

    // If an ant is placed at (0, 0) with a heading, nextVantage shouldn't crash or underflow
    Territory t(home, 0, 4, map);
    Coord vantage(0, 0);
    Coord heading(0, 2);

    for (int step = 0; step < 10; ++step) {
        vantage = t.nextVantage(vantage, heading);
        check(vantage.first >= 0 && vantage.first < 15, "Corner test row in bounds at step " + std::to_string(step));
        check(vantage.second >= 0 && vantage.second < 15, "Corner test col in bounds at step " + std::to_string(step));
    }
}

void testEdgeHomeBaseProgression() {
    MapTemplate map(15, std::vector<int>(15, 0));
    int foodRadius = 2;

    // Test home positions right on the map boundaries and corners
    std::vector<std::pair<Coord, std::string>> edgeHomes = {
        {{0, 7}, "Top edge (0, 7)"},
        {{14, 7}, "Bottom edge (14, 7)"},
        {{7, 0}, "Left edge (7, 0)"},
        {{7, 14}, "Right edge (7, 14)"},
        {{0, 0}, "Top-Left corner (0, 0)"},
        {{0, 14}, "Top-Right corner (0, 14)"},
        {{14, 0}, "Bottom-Left corner (14, 0)"},
        {{14, 14}, "Bottom-Right corner (14, 14)"}
    };

    for (const auto& [home, desc] : edgeHomes) {
        for (int numScouts : {3, 4}) {
            for (int i = 0; i < numScouts; ++i) {
                Territory t(home, i, numScouts, map);
                Coord heading = t.startHeading(foodRadius);

                check(heading != Coord(0, 0),
                      "startHeading is not (0, 0) with " + desc + " for scout " + std::to_string(i));

                Coord vantage = t.startPoint(foodRadius);
                check(vantage.first >= 0 && vantage.first < 15,
                      "startPoint row in bounds with " + desc + " for scout " + std::to_string(i));
                check(vantage.second >= 0 && vantage.second < 15,
                      "startPoint col in bounds with " + desc + " for scout " + std::to_string(i));

                int stationaryCount = 0;
                Coord prevVantage = vantage;
                Coord prevPrevVantage(-1, -1);
                int twoCycleCount = 0;

                for (int step = 0; step < 50; ++step) {
                    Coord next = t.nextVantage(vantage, heading);

                    check(next.first >= 0 && next.first < 15,
                          "nextVantage row in bounds with " + desc + " scout " + std::to_string(i) + " step " + std::to_string(step));
                    check(next.second >= 0 && next.second < 15,
                          "nextVantage col in bounds with " + desc + " scout " + std::to_string(i) + " step " + std::to_string(step));

                    if (next == vantage) {
                        stationaryCount++;
                    } else {
                        stationaryCount = 0;
                    }
                    check(stationaryCount < 3,
                          "Scout " + std::to_string(i) + " froze at vantage with " + desc);

                    if (next == prevPrevVantage) {
                        twoCycleCount++;
                    } else {
                        twoCycleCount = 0;
                    }
                    check(twoCycleCount < 4,
                          "Scout " + std::to_string(i) + " trapped in 2-step oscillation with " + desc);

                    prevPrevVantage = prevVantage;
                    prevVantage = vantage;
                    vantage = next;
                }
            }
        }
    }
}

void testConstructor() {
    MapTemplate map(15, std::vector<int>(15, 0));
    int rows = 15, cols = 15;

    std::vector<std::pair<Coord, std::string>> homePositions = {
        {{7, 7}, "Center (7, 7)"},
        {{0, 7}, "Top edge (0, 7)"},
        {{14, 7}, "Bottom edge (14, 7)"},
        {{7, 0}, "Left edge (7, 0)"},
        {{7, 14}, "Right edge (7, 14)"},
        {{0, 0}, "Top-Left corner (0, 0)"},
        {{14, 14}, "Bottom-Right corner (14, 14)"}
    };
    for (const auto& [home, desc] : homePositions) {
        for (int numScouts = 1; numScouts <= 8; ++numScouts) {
            std::vector<Territory> territories;
            for (int i = 0; i < numScouts; ++i) {
                territories.emplace_back(home, i, numScouts, map);
            }

            // 1. Check boundary continuity
            check(territories[0].start == 0.0, desc + ": Scout 0 start should be 0.0");
            check(territories.back().end == 2.0 * PI, desc + ": Last scout end should be 2*PI");
            for (int i = 0; i < numScouts; ++i) {
                check(territories[i].start <= territories[i].end,
                      desc + ": Scout " + std::to_string(i) + " start <= end");
                if (i > 0) {
                    check(territories[i].start == territories[i - 1].end,
                          desc + ": Scout " + std::to_string(i) + " start matches previous end");
                }
            }

            // 2. Count coords in each territory and ensure all cells are covered
            std::vector<int> counts(numScouts, 0);
            int covered = 0;
            int total = rows * cols - 1;

            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    if (r == home.first && c == home.second) continue;
                    bool inAny = false;
                    for (int i = 0; i < numScouts; ++i) {
                        if (territories[i].within(Coord(r, c))) {
                            counts[i]++;
                            inAny = true;
                        }
                    }
                    if (inAny) covered++;
                }
            }

            check(covered == total, desc + ": All map cells covered (" + std::to_string(covered) + "/" + std::to_string(total) + ")");

            int target = total / numScouts;
            std::cout << desc << " [numScouts=" << numScouts << "] target ~" << target << " cells/scout:\n";
            for (int i = 0; i < numScouts; ++i) {
                check(counts[i] > 0, desc + ": Scout " + std::to_string(i) + " should have coords");
                std::cout << "  Scout " << i << ": " << counts[i] << " cells, angles: ["
                          << territories[i].start << ", " << territories[i].end << "]\n";
            }
        }
    }
}

void testStartPoint() {
    MapTemplate map(15, std::vector<int>(15, 0));
    int rows = 15, cols = 15;
    int foodRadius = 2;

    std::vector<std::pair<Coord, std::string>> homePositions = {
        {{7, 7}, "Center (7, 7)"},
        {{0, 7}, "Top edge (0, 7)"},
        {{14, 7}, "Bottom edge (14, 7)"},
        {{7, 0}, "Left edge (7, 0)"},
        {{7, 14}, "Right edge (7, 14)"},
        {{0, 0}, "Top-Left corner (0, 0)"},
        {{14, 14}, "Bottom-Right corner (14, 14)"}
    };

    for (const auto& [home, desc] : homePositions) {
        for (int numScouts = 1; numScouts <= 8; ++numScouts) {
            for (int i = 0; i < numScouts; ++i) {
                Territory t(home, i, numScouts, map);
                Coord sp = t.startPoint(foodRadius);

                check(sp.first >= 0 && sp.first < rows, desc + ": Scout " + std::to_string(i) + " startPoint row in bounds");
                check(sp.second >= 0 && sp.second < cols, desc + ": Scout " + std::to_string(i) + " startPoint col in bounds");
                check(sp != home, desc + ": Scout " + std::to_string(i) + " startPoint != home");
                check(t.within(sp), desc + ": Scout " + std::to_string(i) + " startPoint is within territory");

                double dist = std::hypot(sp.first - home.first, sp.second - home.second);
                check(dist <= 2.0 * foodRadius + 0.5, desc + ": Scout " + std::to_string(i) + " startPoint distance <= 2*foodRadius");
            }
        }
    }
}

} // namespace

int main() {
    std::cout << "Running Territory constructor tests...\n";
    testConstructor();

    std::cout << "Running Territory startPoint tests...\n";
    testStartPoint();

    std::cout << "Running Territory startHeading tests...\n";
    testStartHeading();

    std::cout << "Running Territory nextVantage tests...\n";
    testNextVantageProgression();
    testCornerVantageBehavior();
    testEdgeHomeBaseProgression();

    std::cout << checks << " checks completed with " << failures << " failures.\n";
    return failures == 0 ? 0 : 1;
}
