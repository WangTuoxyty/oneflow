#include "oneflow/core/operator/operator_util.h"

namespace oneflow {

const size_t GetChannelDim(const std::string& data_format, int32_t NDims) {
  CHECK_GE(NDims, 3);
  if (data_format == "channels_first") {
    return 1;
  } else if (data_format == "channels_last") {
    return NDims - 1;
  } else {
    UNIMPLEMENTED();
  }
}

const size_t DhwOffset(const std::string& data_format) {
  if (data_format == "channels_first") {
    return 2;
  } else if (data_format == "channels_last") {
    return 1;
  } else {
    UNIMPLEMENTED();
  }
}

std::vector<int32_t> Get3DVecInOpConf(const PbRf<int32_t>& field_vals, int32_t NDims) {
  std::vector<int32_t> vec;
  FOR_RANGE(uint8_t, dim, 0, 3) {
    int64_t index = static_cast<int64_t>(dim) - (3 - NDims);
    if (index < 0) {
      vec.push_back(1);
    } else {
      vec.push_back(field_vals.Get(index));
    }
  }
  return vec;
}

int64_t GetInDim(const Shape& shape, const std::string& data_format, int32_t dim, int32_t NDims) {
  int64_t offset = 0;
  if (data_format == "channels_last") {
    offset = 1;
  } else if (data_format == "channels_first") {
    offset = 2;
  } else {
    UNIMPLEMENTED();
  }
  int64_t index = offset + static_cast<int64_t>(dim) - static_cast<int64_t>(3 - NDims);
  if (index < offset) {
    return 1;
  } else {
    return shape.At(index);
  }
}

void GetWindowedOutputSize(int64_t input_size, int32_t filter_size, int32_t dilation_rate,
                           int32_t stride, const std::string& padding_type, int64_t* output_size,
                           int32_t* padding_before, int32_t* padding_after) {
  CHECK_GT(stride, 0);
  CHECK_GE(dilation_rate, 1);

  int32_t effective_filter_size = (filter_size - 1) * dilation_rate + 1;
  if (padding_type == "valid") {
    if (output_size) { *output_size = (input_size - effective_filter_size + stride) / stride; }
    if (padding_before) { *padding_before = 0; }
    if (padding_after) { *padding_after = 0; }
  } else if (padding_type == "same") {
    int64_t tmp_output_size = (input_size + stride - 1) / stride;
    if (output_size) { *output_size = tmp_output_size; }
    const int32_t padding_needed = std::max(
        0,
        static_cast<int32_t>((tmp_output_size - 1) * stride + effective_filter_size - input_size));
    // For odd values of total padding, add more padding at the 'right'
    // side of the given dimension.
    if (padding_before) { *padding_before = padding_needed / 2; }
    if (padding_after) { *padding_after = padding_needed - padding_needed / 2; }
  } else {
    UNIMPLEMENTED();
  }
  if (output_size) { CHECK_GE((*output_size), 0); }
}

void GetWindowedOutputSize(int64_t input_size, int32_t filter_size, int32_t stride,
                           const std::string& padding_type, int64_t* output_size,
                           int32_t* padding_before, int32_t* padding_after) {
  GetWindowedOutputSize(input_size, filter_size, 1, stride, padding_type, output_size,
                        padding_before, padding_after);
}

void GetWindowedOutputSize(int64_t input_size, int32_t filter_size, int32_t stride,
                           const std::string& padding_type, int64_t* output_size,
                           int32_t* padding_size) {
  GetWindowedOutputSize(input_size, filter_size, stride, padding_type, output_size, padding_size,
                        nullptr);
}

void GetDewindowedOutputSize(int64_t input_size, int32_t filter_size, int32_t stride,
                             const std::string& padding_type, int64_t* output_size,
                             int32_t* padding_before, int32_t* padding_after) {
  CHECK_GT(stride, 0);

  if (padding_type == "valid" || padding_type == "same") {
    if (padding_before) { *padding_before = 0; }
    if (padding_after) { *padding_after = 0; }
    if (output_size) { *output_size = (input_size - 1) * stride + filter_size; }
  } else {
    UNIMPLEMENTED();
  }
  if (output_size) { CHECK_GE((*output_size), 0); }
}

void GetDewindowedOutputSize(int64_t input_size, int32_t filter_size, int32_t stride,
                             const std::string& padding_type, int64_t* output_size,
                             int32_t* padding_size) {
  GetDewindowedOutputSize(input_size, filter_size, stride, padding_type, output_size, padding_size,
                          nullptr);
}

void Get3DOutputSize(const std::vector<int64_t>& in, const std::vector<int32_t>& pool_size,
                     const std::vector<int32_t>& strides, const std::string& padding_type,
                     std::vector<int64_t>* out, std::vector<int32_t>* padding) {
  Get3DOutputSize(in, pool_size, strides, padding_type, out, padding, nullptr, nullptr);
}

void Get3DOutputSize(const std::vector<int64_t>& in, const std::vector<int32_t>& pool_size,
                     const std::vector<int32_t>& strides, const std::string& padding_type,
                     std::vector<int64_t>* out, std::vector<int32_t>* padding_before,
                     std::vector<int32_t>* padding_after) {
  Get3DOutputSize(in, pool_size, strides, padding_type, out, padding_before, padding_after,
                  nullptr);
}

void Get3DOutputSize(const std::vector<int64_t>& in, const std::vector<int32_t>& pool_size,
                     const std::vector<int32_t>& strides, const std::string& padding_type,
                     std::vector<int64_t>* out, std::vector<int32_t>* padding_before,
                     std::vector<int32_t>* padding_after, std::vector<int32_t>* dilation_rate) {
  CHECK(out);
  out->clear();
  out->resize(3);
  if (padding_before) {
    padding_before->clear();
    padding_before->resize(3);
  }
  if (padding_after) {
    padding_after->clear();
    padding_after->resize(3);
  }
  FOR_RANGE(size_t, i, 0, 3) {
    int64_t* out_ptr = &(*out).at(i);
    int32_t* padding_before_ptr = padding_before ? (&(*padding_before).at(i)) : nullptr;
    int32_t* padding_after_ptr = padding_after ? (&(*padding_after).at(i)) : nullptr;
    if (dilation_rate) {
      GetWindowedOutputSize(in.at(i), pool_size.at(i), dilation_rate->at(i), strides.at(i),
                            padding_type, out_ptr, padding_before_ptr, padding_after_ptr);
    } else {
      GetWindowedOutputSize(in.at(i), pool_size.at(i), strides.at(i), padding_type, out_ptr,
                            padding_before_ptr, padding_after_ptr);
    }
  }
}

float GetResizeScale(const int64_t input_size, const int64_t output_size,
                     const bool align_corners) {
  return (align_corners && output_size > 1) ? (input_size - 1) / static_cast<float>(output_size - 1)
                                            : input_size / static_cast<float>(output_size);
}

}  // namespace oneflow
