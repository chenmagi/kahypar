/*******************************************************************************
 * This file is part of KaHyPar.
 *
 * Copyright (C) 2018 Sebastian Schlag <sebastian.schlag@kit.edu>
 *
 * KaHyPar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * KaHyPar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KaHyPar.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include <memory>

#include "gmock/gmock.h"

#include "include/libkahypar.h"

#include "kahypar/macros.h"

#include "tests/io/hypergraph_io_test_fixtures.h"

using ::testing::Eq;
using ::testing::ContainerEq;

namespace kahypar {
TEST(KaHyPar, CanBeCalledViaInterface) {
  kahypar_context_t* context = kahypar_context_new();

  kahypar_configure_context_from_file(context, "../../../config/km1_direct_kway_sea18.ini");

  const kahypar_hypernode_id_t num_vertices = 7;
  const kahypar_hyperedge_id_t num_hyperedges = 4;

  std::unique_ptr<kahypar_hyperedge_weight_t[]> hyperedge_weights =
    std::make_unique<kahypar_hyperedge_weight_t[]>(4);

  // force the cut to contain hyperedge 0 and 2
  hyperedge_weights[0] = 1;
  hyperedge_weights[1] = 1000;
  hyperedge_weights[2] = 1;
  hyperedge_weights[3] = 1000;

  std::unique_ptr<size_t[]> hyperedge_indices =
    std::make_unique<size_t[]>(5);

  hyperedge_indices[0] = 0;
  hyperedge_indices[1] = 2;
  hyperedge_indices[2] = 6;
  hyperedge_indices[3] = 9;
  hyperedge_indices[4] = 12;

  std::unique_ptr<kahypar_hypernode_id_t[]> hyperedges =
    std::make_unique<kahypar_hypernode_id_t[]>(12);

  // hypergraph from hMetis manual page 14
  hyperedges[0] = 0;
  hyperedges[1] = 2;
  hyperedges[2] = 0;
  hyperedges[3] = 1;
  hyperedges[4] = 3;
  hyperedges[5] = 4;
  hyperedges[6] = 3;
  hyperedges[7] = 4;
  hyperedges[8] = 6;
  hyperedges[9] = 2;
  hyperedges[10] = 5;
  hyperedges[11] = 6;

  const double imbalance = 0.03;
  const kahypar_partition_id_t k = 2;
  kahypar_hyperedge_weight_t objective = 0;

  std::vector<kahypar_partition_id_t> partition(num_vertices, -1);

  kahypar_partition(num_vertices, num_hyperedges,
                    imbalance, k,
                    /*vertex_weights */ nullptr, hyperedge_weights.get(),
                    hyperedge_indices.get(), hyperedges.get(),
                    &objective, context, partition.data());


  std::vector<kahypar_partition_id_t> correct_solution({ 0, 0, 1, 0, 0, 1, 1 });
  std::vector<kahypar_partition_id_t> correct_solution2({ 1, 1, 0, 1, 1, 0, 0 });
  ASSERT_THAT(partition, AnyOf(::testing::ContainerEq(correct_solution),
                               ::testing::ContainerEq(correct_solution2)));
  ASSERT_EQ(objective, 2);

  kahypar_context_free(context);
}

TEST(KaHyPar, SupportsIndividualBlockWeightsViaInterface) {
  kahypar_context_t* context = kahypar_context_new();
  kahypar_configure_context_from_file(context, "../../../config/km1_direct_kway_sea18.ini");

  reinterpret_cast<kahypar::Context*>(context)->preprocessing.enable_community_detection = false;

  HypernodeID num_hypernodes = 0;
  HyperedgeID num_hyperedges = 0;

  size_t* index_ptr = nullptr;
  kahypar_hypernode_id_t* hyperedges_ptr = nullptr;

  kahypar_hyperedge_weight_t* hyperedge_weights_ptr = nullptr;
  kahypar_hypernode_weight_t* vertex_weights_ptr = nullptr;

  const std::string filename("test_instances/ISPD98_ibm01.hgr");
  kahypar_read_hypergraph_from_file(filename.c_str(),
                                    &num_hypernodes,
                                    &num_hyperedges,
                                    &index_ptr,
                                    &hyperedges_ptr,
                                    &hyperedge_weights_ptr,
                                    &vertex_weights_ptr);

  const double imbalance = 0.0;
  const kahypar_partition_id_t num_blocks = 6;
  const std::array<kahypar_hypernode_weight_t, num_blocks> max_part_weights = { 2750, 1000, 3675, 2550, 2550, 250 };

  kahypar_hyperedge_weight_t objective = 0;
  std::vector<kahypar_partition_id_t> partition(num_hypernodes, -1);

  kahypar_partition_using_custom_block_weights(num_hypernodes,
                                               num_hyperedges,
                                               imbalance,
                                               num_blocks,
                                               /*vertex_weights */ nullptr,
                                               /*hyperedge_weights */ nullptr,
                                               index_ptr,
                                               hyperedges_ptr,
                                               max_part_weights.data(),
                                               &objective,
                                               context,
                                               partition.data());

  Hypergraph verification_hypergraph(kahypar::io::createHypergraphFromFile(filename, num_blocks));

  for (const HypernodeID& hn : verification_hypergraph.nodes()) {
    verification_hypergraph.setNodePart(hn, partition[hn]);
  }

  ASSERT_LE(verification_hypergraph.partWeight(0), max_part_weights[0]);
  ASSERT_LE(verification_hypergraph.partWeight(1), max_part_weights[1]);
  ASSERT_LE(verification_hypergraph.partWeight(2), max_part_weights[2]);
  ASSERT_LE(verification_hypergraph.partWeight(3), max_part_weights[3]);
  ASSERT_LE(verification_hypergraph.partWeight(4), max_part_weights[4]);
  ASSERT_LE(verification_hypergraph.partWeight(5), max_part_weights[5]);

  ASSERT_EQ(objective, metrics::km1(verification_hypergraph));

  kahypar_context_free(context);
}
namespace io {
TEST_F(AnUnweightedHypergraphFile, CanBeParsedIntoAHypergraph) {
  HypernodeID num_hypernodes = 0;
  HyperedgeID num_hyperedges = 0;

  size_t* index_ptr = nullptr;
  kahypar_hypernode_id_t* hyperedges_ptr = nullptr;

  kahypar_hyperedge_weight_t* hyperedge_weights_ptr = nullptr;
  kahypar_hypernode_weight_t* vertex_weights_ptr = nullptr;

  kahypar_read_hypergraph_from_file(_filename.c_str(),
                                    &num_hypernodes,
                                    &num_hyperedges,
                                    &index_ptr,
                                    &hyperedges_ptr,
                                    &hyperedge_weights_ptr,
                                    &vertex_weights_ptr);


  ASSERT_TRUE(verifyEquivalenceWithoutPartitionInfo(Hypergraph(_control_num_hypernodes,
                                                               _control_num_hyperedges,
                                                               _control_index_vector,
                                                               _control_edge_vector),
                                                    Hypergraph(num_hypernodes,
                                                               num_hyperedges,
                                                               index_ptr,
                                                               hyperedges_ptr)));
  delete[] index_ptr;
  delete[] hyperedges_ptr;
}

TEST_F(AHypergraphFileWithHyperedgeWeights, CanBeParsedIntoAHypergraph) {
  HypernodeID num_hypernodes = 0;
  HyperedgeID num_hyperedges = 0;

  size_t* index_ptr = nullptr;
  kahypar_hypernode_id_t* hyperedges_ptr = nullptr;

  kahypar_hyperedge_weight_t* hyperedge_weights_ptr = nullptr;
  kahypar_hypernode_weight_t* vertex_weights_ptr = nullptr;

  kahypar_read_hypergraph_from_file(_filename.c_str(),
                                    &num_hypernodes,
                                    &num_hyperedges,
                                    &index_ptr,
                                    &hyperedges_ptr,
                                    &hyperedge_weights_ptr,
                                    &vertex_weights_ptr);


  ASSERT_TRUE(verifyEquivalenceWithoutPartitionInfo(Hypergraph(_control_num_hypernodes,
                                                               _control_num_hyperedges,
                                                               _control_index_vector,
                                                               _control_edge_vector,
                                                               2,
                                                               &_control_hyperedge_weights),
                                                    Hypergraph(num_hypernodes,
                                                               num_hyperedges,
                                                               index_ptr,
                                                               hyperedges_ptr,
                                                               2,
                                                               hyperedge_weights_ptr)));
  delete[] index_ptr;
  delete[] hyperedges_ptr;
  delete[] hyperedge_weights_ptr;
}

TEST_F(AHypergraphFileWithHypernodeWeights, CanBeParsedIntoAHypergraph) {
  HypernodeID num_hypernodes = 0;
  HyperedgeID num_hyperedges = 0;

  size_t* index_ptr = nullptr;
  kahypar_hypernode_id_t* hyperedges_ptr = nullptr;

  kahypar_hyperedge_weight_t* hyperedge_weights_ptr = nullptr;
  kahypar_hypernode_weight_t* vertex_weights_ptr = nullptr;

  kahypar_read_hypergraph_from_file(_filename.c_str(),
                                    &num_hypernodes,
                                    &num_hyperedges,
                                    &index_ptr,
                                    &hyperedges_ptr,
                                    &hyperedge_weights_ptr,
                                    &vertex_weights_ptr);


  ASSERT_TRUE(verifyEquivalenceWithoutPartitionInfo(Hypergraph(_control_num_hypernodes,
                                                               _control_num_hyperedges,
                                                               _control_index_vector,
                                                               _control_edge_vector,
                                                               2,
                                                               nullptr,
                                                               &_control_hypernode_weights),
                                                    Hypergraph(num_hypernodes,
                                                               num_hyperedges,
                                                               index_ptr,
                                                               hyperedges_ptr,
                                                               2,
                                                               nullptr,
                                                               vertex_weights_ptr)));
  delete[] index_ptr;
  delete[] hyperedges_ptr;
  delete[] vertex_weights_ptr;
}

TEST_F(AHypergraphFileWithHypernodeAndHyperedgeWeights, CanBeParsedIntoAHypergraph) {
  HypernodeID num_hypernodes = 0;
  HyperedgeID num_hyperedges = 0;

  size_t* index_ptr = nullptr;
  kahypar_hypernode_id_t* hyperedges_ptr = nullptr;

  kahypar_hyperedge_weight_t* hyperedge_weights_ptr = nullptr;
  kahypar_hypernode_weight_t* vertex_weights_ptr = nullptr;

  kahypar_read_hypergraph_from_file(_filename.c_str(),
                                    &num_hypernodes,
                                    &num_hyperedges,
                                    &index_ptr,
                                    &hyperedges_ptr,
                                    &hyperedge_weights_ptr,
                                    &vertex_weights_ptr);


  ASSERT_TRUE(verifyEquivalenceWithoutPartitionInfo(Hypergraph(_control_num_hypernodes,
                                                               _control_num_hyperedges,
                                                               _control_index_vector,
                                                               _control_edge_vector,
                                                               2,
                                                               &_control_hyperedge_weights,
                                                               &_control_hypernode_weights),
                                                    Hypergraph(num_hypernodes,
                                                               num_hyperedges,
                                                               index_ptr,
                                                               hyperedges_ptr,
                                                               2,
                                                               hyperedge_weights_ptr,
                                                               vertex_weights_ptr)));
  delete[] index_ptr;
  delete[] hyperedges_ptr;
  delete[] hyperedge_weights_ptr;
  delete[] vertex_weights_ptr;
}
}  // namespace io
}  // namespace kahypar
