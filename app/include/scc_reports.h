#ifndef SCC_SCC_REPORTS_H
#define SCC_SCC_REPORTS_H

#include <vector>
#include <functional>
#include <tuple>
#include <sccalgorithms/scc_algorithms.h>
#include <sccalgorithms/scc_algorithm.h>
#include <sccalgorithms/scc_algorithm.h>
#include <numeric>
#include <chrono>
#include <string>
#include <boost/graph/erdos_renyi_generator.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/adjacency_list.hpp>
#include "get_rss.h"

typedef std::tuple<unsigned, unsigned, unsigned, std::vector<std::tuple<unsigned, bool> > > scc_record;

typedef std::tuple<unsigned, unsigned, unsigned, size_t, size_t, size_t, std::string> scc_record_memory;

template <class Graph, class ComponentMap = typename boost::iterator_property_map<typename std::vector<int>::iterator, typename boost::property_map<Graph, boost::vertex_index_t>::const_type> >
class run_algorithm_class{
public:
    run_algorithm_class(Graph &randomGraph, ComponentMap &componentMap, sccalgorithms::scc_algorithm<Graph, ComponentMap> &algorithm) : randomGraph(randomGraph),
                                                                                                                                        componentMap(componentMap),
                                                                                                                                        algorithm(algorithm){}

    inline unsigned int run()
    {
        return algorithm(randomGraph, componentMap);
    }

private:
    Graph randomGraph;
    ComponentMap componentMap;
    sccalgorithms::scc_algorithm<Graph, ComponentMap> algorithm;


};

/**
 * Generates a report running algorithms provided on random graphs
 */
template <class Graph, class ComponentMap = typename boost::iterator_property_map<typename std::vector<int>::iterator, typename boost::property_map<Graph, boost::vertex_index_t>::const_type> >
class scc_reports {
public:
    scc_reports(unsigned minV, unsigned maxV, unsigned offsetV, float minDensity,
                float maxDensity, float offsetDensity) : minV(minV), maxV(maxV), offsetV(offsetV),
                                                                minDensity(minDensity), maxDensity(maxDensity),
                                                                offsetDensity(offsetDensity) {}

    inline
    std::vector<scc_record>
    run(std::vector<sccalgorithms::scc_algorithm<Graph, ComponentMap> > & algorithms) {

        typedef typename boost::erdos_renyi_iterator<boost::minstd_rand, Graph> ERGen;

        // save records here
        std::vector<scc_record> results;

        boost::minstd_rand gen;

        for (unsigned i=std::max(2u, minV); i<=maxV; i+=1) { // for each number of verteces
            for (float j=minDensity; j<=maxDensity; j+=offsetDensity) { // for each density value
                Graph randomGraph(ERGen(gen, i, j), ERGen(), i); // creates a random graph

                // computes the number of sccs from boost tarjan algorithm
                std::vector<int> component_boost(boost::num_vertices(randomGraph));
                unsigned boost_num_sccs = boost::strong_components(randomGraph,
                                                                   boost::make_iterator_property_map(component_boost.begin(), boost::get(boost::vertex_index, randomGraph)));

                std::vector<std::tuple<unsigned, bool> > algorithms_result;
                for (auto & algorithm : algorithms) { // for each algorithm that computes scc given
                    std::vector<int> component(boost::num_vertices(randomGraph));
                    auto componentMap = boost::make_iterator_property_map(component.begin(), boost::get(boost::vertex_index, randomGraph));

                    std::chrono::high_resolution_clock::time_point t1, t2;
                    t1 = std::chrono::high_resolution_clock::now();

                    unsigned algorithm_num_sccs = algorithm(randomGraph, componentMap);

                    t2 = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

                    algorithms_result.push_back(std::make_tuple(duration, algorithm_num_sccs == boost_num_sccs && component == component_boost));
                }

                results.push_back(std::make_tuple(i, boost::num_edges(randomGraph), boost_num_sccs, algorithms_result));
            }
        }

        return results;
    };

private:
    unsigned minV;
    unsigned maxV;
    unsigned offsetV;
    float minDensity;
    float maxDensity;
    float offsetDensity;
};

/**
 * Generates memory report running one provided algorithm on a random graph
 */
template <class Graph, class ComponentMap = typename boost::iterator_property_map<typename std::vector<int>::iterator, typename boost::property_map<Graph, boost::vertex_index_t>::const_type> >
class scc_reports_memory {
public:
    scc_reports_memory(unsigned numberOfV, float edgeDensity) : numberOfV(numberOfV),
                                                                                edgeDensity(edgeDensity) {}

    inline
    scc_record_memory
    run(sccalgorithms::scc_algorithm<Graph, ComponentMap> & algorithm) {
        typedef typename boost::erdos_renyi_iterator<boost::minstd_rand, Graph> ERGen;

        boost::minstd_rand gen;
        numberOfV = std::max(2u, numberOfV);
        Graph randomGraph(ERGen(gen, numberOfV, edgeDensity), ERGen(), edgeDensity); // creates a random graph


        std::vector<int> component(boost::num_vertices(randomGraph));
        auto componentMap = boost::make_iterator_property_map(component.begin(), boost::get(boost::vertex_index, randomGraph));

        size_t tare = getCurrentRSS();
        //unsigned num_sccs = algorithm(randomGraph, componentMap);
        unsigned num_sccs = run_algorithm_class<sccalgorithms::DirectedGraph>(randomGraph, componentMap, algorithm).run();

        size_t algorithmPeak = getPeakRSS();

        return std::make_tuple(numberOfV, boost::num_edges(randomGraph), num_sccs, tare, algorithmPeak, algorithmPeak - tare, algorithm.getName());
    }
private:
    unsigned numberOfV;
    float edgeDensity;
};

class report_formatter {
public:
    report_formatter(const std::vector<scc_record> & records,
                     const std::vector<std::string> & algorithms_names) : records(records), algorithms_names(algorithms_names) {}

    virtual void print_header(std::ostream &) const = 0;
    virtual void print_record(std::ostream &, const scc_record &) const = 0;

    friend std::ostream & operator<<(std::ostream & os, const report_formatter & rf) {
        rf.print_header(os);
        for (const auto & record : rf.records)
            rf.print_record(os, record);

        return os;
    }

private:
    const std::vector<scc_record> & records;

protected:
    const std::vector<std::string> & algorithms_names;
};

#endif //SCC_SCC_REPORTS_H
