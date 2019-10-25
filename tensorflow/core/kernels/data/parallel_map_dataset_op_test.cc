/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/core/kernels/data/parallel_map_dataset_op.h"

#include "tensorflow/core/kernels/data/dataset_test_base.h"

namespace tensorflow {
namespace data {
namespace {

constexpr char kNodeName[] = "parallel_map_dataset";

class ParallelMapDatasetParams : public DatasetParams {
 public:
  template <typename T>
  ParallelMapDatasetParams(T input_dataset_params,
                           std::vector<Tensor> other_arguments,
                           int num_parallel_calls,
                           FunctionDefHelper::AttrValueWrapper func,
                           std::vector<FunctionDef> func_lib,
                           DataTypeVector type_arguments,
                           DataTypeVector output_dtypes,
                           std::vector<PartialTensorShape> output_shapes,
                           bool use_inter_op_parallelism, bool sloppy,
                           bool preserve_cardinality, string node_name)
      : DatasetParams(std::move(output_dtypes), std::move(output_shapes),
                      std::move(node_name)),
        other_arguments_(std::move(other_arguments)),
        num_parallel_calls_(num_parallel_calls),
        func_(std::move(func)),
        func_lib_(std::move(func_lib)),
        type_arguments_(std::move(type_arguments)),
        use_inter_op_parallelism_(use_inter_op_parallelism),
        sloppy_(sloppy),
        preserve_cardinality_(preserve_cardinality) {
    input_dataset_params_.push_back(absl::make_unique<T>(input_dataset_params));
    iterator_prefix_ =
        name_utils::IteratorPrefix(input_dataset_params.dataset_type(),
                                   input_dataset_params.iterator_prefix());
  }

  std::vector<Tensor> GetInputTensors() const override {
    auto input_tensors = other_arguments_;
    input_tensors.emplace_back(
        CreateTensor<int32>(TensorShape({}), {num_parallel_calls_}));
    return input_tensors;
  }

  Status GetInputNames(std::vector<string>* input_names) const override {
    input_names->emplace_back(ParallelMapDatasetOp::kInputDataset);
    for (int i = 0; i < other_arguments_.size(); ++i) {
      input_names->emplace_back(
          absl::StrCat(ParallelMapDatasetOp::kOtherArguments, "_", i));
    }
    input_names->emplace_back(ParallelMapDatasetOp::kNumParallelCalls);
    return Status::OK();
  }

  Status GetAttributes(AttributeVector* attr_vector) const override {
    *attr_vector = {
        {ParallelMapDatasetOp::kFunc, func_},
        {ParallelMapDatasetOp::kTarguments, type_arguments_},
        {ParallelMapDatasetOp::kOutputShapes, output_shapes_},
        {ParallelMapDatasetOp::kOutputTypes, output_dtypes_},
        {ParallelMapDatasetOp::kUseInterOpParallelism,
         use_inter_op_parallelism_},
        {ParallelMapDatasetOp::kSloppy, sloppy_},
        {ParallelMapDatasetOp::kPreserveCardinality, preserve_cardinality_}};
    return Status::OK();
  }

  string dataset_type() const override {
    return ParallelMapDatasetOp::kDatasetType;
  }

  std::vector<FunctionDef> func_lib() const override { return func_lib_; }

  bool sloppy() const override { return sloppy_; }

 private:
  std::vector<Tensor> other_arguments_;
  int num_parallel_calls_;
  FunctionDefHelper::AttrValueWrapper func_;
  std::vector<FunctionDef> func_lib_;
  DataTypeVector type_arguments_;
  bool use_inter_op_parallelism_;
  bool sloppy_;
  bool preserve_cardinality_;
};

class ParallelMapDatasetOpTest : public DatasetOpsTestBaseV2 {};

FunctionDefHelper::AttrValueWrapper MapFunc(const string& func_name,
                                            const DataType& dtype) {
  return FunctionDefHelper::FunctionRef(func_name, {{"T", dtype}});
}

// test case 1: num_parallel_calls = 1, use_inter_op_parallelism = false,
// sloppy = false, preserve_cardinality = false, MapFunc = XTimesTwo
ParallelMapDatasetParams ParallelMapDatasetParams1() {
  return ParallelMapDatasetParams(RangeDatasetParams(0, 10, 3),
                                  /*other_arguments=*/{},
                                  /*num_parallel_calls=*/1,
                                  /*func=*/MapFunc("XTimesTwo", DT_INT64),
                                  /*func_lib*/ {test::function::XTimesTwo()},
                                  /*type_arguments=*/{},
                                  /*output_dtypes=*/{DT_INT64},
                                  /*output_shapes=*/{PartialTensorShape({})},
                                  /*use_inter_op_parallelism=*/false,
                                  /*sloppy=*/false,
                                  /*preserve_cardinality=*/false,
                                  /*node_name=*/kNodeName);
}

// test case 2: num_parallel_calls = 2, use_inter_op_parallelism = true,
// sloppy = true, preserve_cardinality = true, MapFunc = XTimesTwo
ParallelMapDatasetParams ParallelMapDatasetParams2() {
  return ParallelMapDatasetParams(RangeDatasetParams(0, 10, 3),
                                  /*other_arguments=*/{},
                                  /*num_parallel_calls=*/2,
                                  /*func=*/MapFunc("XTimesTwo", DT_INT64),
                                  /*func_lib*/ {test::function::XTimesTwo()},
                                  /*type_arguments=*/{},
                                  /*output_dtypes=*/{DT_INT64},
                                  /*output_shapes=*/{PartialTensorShape({})},
                                  /*use_inter_op_parallelism=*/true,
                                  /*sloppy=*/true,
                                  /*preserve_cardinality=*/true,
                                  /*node_name=*/kNodeName);
}

// test case 3: num_parallel_calls = 3, use_inter_op_parallelism = true,
// sloppy = false, preserve_cardinality = false, MapFunc = XTimesFour
ParallelMapDatasetParams ParallelMapDatasetParams3() {
  return ParallelMapDatasetParams(
      RangeDatasetParams(0, 10, 3),
      /*other_arguments=*/{},
      /*num_parallel_calls=*/3,
      /*func=*/MapFunc("XTimesFour", DT_INT64),
      /*func_lib*/ {test::function::XTimesTwo(), test::function::XTimesFour()},
      /*type_arguments=*/{},
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({})},
      /*use_inter_op_parallelism=*/true,
      /*sloppy=*/false,
      /*preserve_cardinality=*/false,
      /*node_name=*/kNodeName);
}

// test case 4: num_parallel_calls = 4, use_inter_op_parallelism = false,
// sloppy = false, preserve_cardinality = false, MapFunc = XTimesTwo
ParallelMapDatasetParams ParallelMapDatasetParams4() {
  return ParallelMapDatasetParams(RangeDatasetParams(0, 10, 3),
                                  /*other_arguments=*/{},
                                  /*num_parallel_calls=*/4,
                                  /*func=*/MapFunc("XTimesTwo", DT_INT64),
                                  /*func_lib*/ {test::function::XTimesTwo()},
                                  /*type_arguments=*/{},
                                  /*output_dtypes=*/{DT_INT64},
                                  /*output_shapes=*/{PartialTensorShape({})},
                                  /*use_inter_op_parallelism=*/false,
                                  /*sloppy=*/false,
                                  /*preserve_cardinality=*/false,
                                  /*node_name=*/kNodeName);
}

// test case 5: num_parallel_calls = kAutotune, use_inter_op_parallelism = true,
// sloppy = true, preserve_cardinality = true, MapFunc = XTimesFour
ParallelMapDatasetParams ParallelMapDatasetParams5() {
  return ParallelMapDatasetParams(
      RangeDatasetParams(0, 10, 3),
      /*other_arguments=*/{},
      /*num_parallel_calls=*/model::kAutotune,
      /*func=*/MapFunc("XTimesFour", DT_INT64),
      /*func_lib*/ {test::function::XTimesTwo(), test::function::XTimesFour()},
      /*type_arguments=*/{},
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({})},
      /*use_inter_op_parallelism=*/true,
      /*sloppy=*/true,
      /*preserve_cardinality=*/true,
      /*node_name=*/kNodeName);
}

// test case 6: num_parallel_calls = 4, use_inter_op_parallelism = true,
// sloppy = false, preserve_cardinality = false, MapFunc = XTimesFour
ParallelMapDatasetParams ParallelMapDatasetParams6() {
  return ParallelMapDatasetParams(
      RangeDatasetParams(0, 10, 3),
      /*other_arguments=*/{},
      /*num_parallel_calls=*/4,
      /*func=*/MapFunc("XTimesFour", DT_INT64),
      /*func_lib*/ {test::function::XTimesTwo(), test::function::XTimesFour()},
      /*type_arguments=*/{},
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({})},
      /*use_inter_op_parallelism=*/true,
      /*sloppy=*/false,
      /*preserve_cardinality=*/false,
      /*node_name=*/kNodeName);
}

// TODO(feihugis): make this test case work.
// test case 7: num_parallel_calls = 2, use_inter_op_parallelism = false,
// sloppy = false, preserve_cardinality = false, MapFunc = XTimesFour
ParallelMapDatasetParams ParallelMapDatasetParams7() {
  return ParallelMapDatasetParams(
      RangeDatasetParams(0, 10, 3),
      /*other_arguments=*/{},
      /*num_parallel_calls=*/2,
      /*func=*/MapFunc("XTimesFour", DT_INT64),
      /*func_lib*/ {test::function::XTimesTwo(), test::function::XTimesFour()},
      /*type_arguments=*/{},
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({})},
      /*use_inter_op_parallelism=*/false,
      /*sloppy=*/false,
      /*preserve_cardinality=*/false,
      /*node_name=*/kNodeName);
}

// TODO(feihugis): make this test case work.
// test case 8: num_parallel_calls = kAutotune, use_inter_op_parallelism =
// false, sloppy = true, preserve_cardinality = true, MapFunc = XTimesFour
ParallelMapDatasetParams ParallelMapDatasetParams8() {
  return ParallelMapDatasetParams(
      RangeDatasetParams(0, 10, 3),
      /*other_arguments=*/{},
      /*num_parallel_calls=*/model::kAutotune,
      /*func=*/MapFunc("XTimesFour", DT_INT64),
      /*func_lib*/ {test::function::XTimesTwo(), test::function::XTimesFour()},
      /*type_arguments=*/{},
      /*output_dtypes=*/{DT_INT64},
      /*output_shapes=*/{PartialTensorShape({})},
      /*use_inter_op_parallelism=*/false,
      /*sloppy=*/true,
      /*preserve_cardinality=*/true,
      /*node_name=*/kNodeName);
}

ParallelMapDatasetParams ParallelMapDatasetParamsWithInvalidNumParallelCalls() {
  return ParallelMapDatasetParams(RangeDatasetParams(0, 10, 3),
                                  /*other_arguments=*/{},
                                  /*num_parallel_calls=*/-4,
                                  /*func=*/MapFunc("XTimesTwo", DT_INT64),
                                  /*func_lib*/ {test::function::XTimesTwo()},
                                  /*type_arguments=*/{},
                                  /*output_dtypes=*/{DT_INT64},
                                  /*output_shapes=*/{PartialTensorShape({})},
                                  /*use_inter_op_parallelism=*/true,
                                  /*sloppy=*/true,
                                  /*preserve_cardinality=*/true,
                                  /*node_name=*/kNodeName);
}

std::vector<GetNextTestCase<ParallelMapDatasetParams>> GetNextTestCases() {
  return {{/*dataset_params=*/ParallelMapDatasetParams1(),
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {6}, {12}, {18}})},
          {/*dataset_params=*/ParallelMapDatasetParams2(),
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {6}, {12}, {18}})},
          {/*dataset_params=*/ParallelMapDatasetParams3(),
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {12}, {24}, {36}})},
          {/*dataset_params=*/ParallelMapDatasetParams4(),
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {6}, {12}, {18}})},
          {/*dataset_params=*/ParallelMapDatasetParams5(),
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {12}, {24}, {36}})},
          {/*dataset_params=*/
           ParallelMapDatasetParams6(),
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {12}, {24}, {36}})}};
}

ITERATOR_GET_NEXT_TEST_P(ParallelMapDatasetOpTest, ParallelMapDatasetParams,
                         GetNextTestCases())

TEST_F(ParallelMapDatasetOpTest, DatasetNodeName) {
  auto dataset_params = ParallelMapDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetNodeName(dataset_params.node_name()));
}

TEST_F(ParallelMapDatasetOpTest, DatasetTypeString) {
  auto dataset_params = ParallelMapDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetTypeString(
      name_utils::OpName(ParallelMapDatasetOp::kDatasetType)));
}

TEST_F(ParallelMapDatasetOpTest, DatasetOutputDtypes) {
  auto dataset_params = ParallelMapDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetOutputDtypes({DT_INT64}));
}

TEST_F(ParallelMapDatasetOpTest, DatasetOutputShapes) {
  auto dataset_params = ParallelMapDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckDatasetOutputShapes({PartialTensorShape({})}));
}

std::vector<CardinalityTestCase<ParallelMapDatasetParams>>
CardinalityTestCases() {
  return {{/*dataset_params=*/ParallelMapDatasetParams1(),
           /*expected_cardinality=*/4},
          {/*dataset_params=*/ParallelMapDatasetParams2(),
           /*expected_cardinality=*/4},
          {/*dataset_params=*/ParallelMapDatasetParams3(),
           /*expected_cardinality=*/4},
          {/*dataset_params=*/ParallelMapDatasetParams4(),
           /*expected_cardinality=*/4},
          {/*dataset_params=*/ParallelMapDatasetParams5(),
           /*expected_cardinality=*/4},
          {/*dataset_params=*/ParallelMapDatasetParams6(),
           /*expected_cardinality=*/4}};
}

DATASET_CARDINALITY_TEST_P(ParallelMapDatasetOpTest, ParallelMapDatasetParams,
                           CardinalityTestCases())

TEST_F(ParallelMapDatasetOpTest, IteratorOutputDtypes) {
  auto dataset_params = ParallelMapDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckIteratorOutputDtypes({DT_INT64}));
}

TEST_F(ParallelMapDatasetOpTest, IteratorOutputShapes) {
  auto dataset_params = ParallelMapDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckIteratorOutputShapes({PartialTensorShape({})}));
}

TEST_F(ParallelMapDatasetOpTest, IteratorPrefix) {
  auto dataset_params = ParallelMapDatasetParams1();
  TF_ASSERT_OK(Initialize(dataset_params));
  TF_ASSERT_OK(CheckIteratorPrefix(name_utils::IteratorPrefix(
      ParallelMapDatasetOp::kDatasetType, dataset_params.iterator_prefix())));
}

std::vector<IteratorSaveAndRestoreTestCase<ParallelMapDatasetParams>>
IteratorSaveAndRestoreTestCases() {
  return {{/*dataset_params=*/ParallelMapDatasetParams1(),
           /*breakpoints=*/{0, 1, 5},
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {6}, {12}, {18}})},
          {/*dataset_params=*/ParallelMapDatasetParams2(),
           /*breakpoints=*/{0, 1, 5},
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {6}, {12}, {18}})},
          {/*dataset_params=*/ParallelMapDatasetParams3(),
           /*breakpoints=*/{0, 1, 5},
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {12}, {24}, {36}})},
          {/*dataset_params=*/ParallelMapDatasetParams4(),
           /*breakpoints=*/{0, 1, 5},
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {6}, {12}, {18}})},
          {/*dataset_params=*/ParallelMapDatasetParams5(),
           /*breakpoints=*/{0, 1, 5},
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {12}, {24}, {36}})},
          {/*dataset_params=*/
           ParallelMapDatasetParams6(),
           /*breakpoints=*/{0, 1, 5},
           /*expected_outputs=*/
           CreateTensors<int64>(TensorShape{}, {{0}, {12}, {24}, {36}})}};
}

class ParameterizedIteratorSaveAndRestoreTest
    : public ParallelMapDatasetOpTest,
      public ::testing::WithParamInterface<
          IteratorSaveAndRestoreTestCase<ParallelMapDatasetParams>> {};

TEST_P(ParameterizedIteratorSaveAndRestoreTest, Roundtrip) {
  auto test_case = GetParam();
  TF_ASSERT_OK(Initialize(test_case.dataset_params));

  std::unique_ptr<SerializationContext> serialization_ctx;
  TF_ASSERT_OK(CreateSerializationContext(&serialization_ctx));
  bool end_of_sequence = false;
  std::vector<Tensor> out_tensors;
  int cur_iteration = 0;
  for (int breakpoint : test_case.breakpoints) {
    VariantTensorData data;
    VariantTensorDataWriter writer(&data);
    TF_EXPECT_OK(iterator_->Save(serialization_ctx.get(), &writer));
    TF_EXPECT_OK(writer.Flush());
    VariantTensorDataReader reader(&data);
    TF_EXPECT_OK(RestoreIterator(iterator_ctx_.get(), &reader,
                                 test_case.dataset_params.iterator_prefix(),
                                 *dataset_, &iterator_));

    while (cur_iteration <= breakpoint) {
      std::vector<Tensor> next;
      TF_EXPECT_OK(
          iterator_->GetNext(iterator_ctx_.get(), &next, &end_of_sequence));
      out_tensors.insert(out_tensors.end(), next.begin(), next.end());
      cur_iteration++;
    }
  }

  TF_EXPECT_OK(
      ExpectEqual(out_tensors, test_case.expected_outputs,
                  /*compare_order=*/!test_case.dataset_params.sloppy()));
}

INSTANTIATE_TEST_CASE_P(ParallelMapDatasetOpTest,
                        ParameterizedIteratorSaveAndRestoreTest,
                        ::testing::ValuesIn(IteratorSaveAndRestoreTestCases()));

TEST_F(ParallelMapDatasetOpTest, InvalidNumParallelCalls) {
  auto dataset_params = ParallelMapDatasetParamsWithInvalidNumParallelCalls();
  EXPECT_EQ(Initialize(dataset_params).code(),
            tensorflow::error::INVALID_ARGUMENT);
}

}  // namespace
}  // namespace data
}  // namespace tensorflow
