#define RCPP_NO_RTTI
#define RCPP_NO_SUGAR
#include <Rcpp.h>

#include "arrayIO/binaryfile.h"
#include "arrayIO/bp128.h"
#include "arrayIO/hdf5.h"
#include "arrayIO/vector.h"
#include "bitpacking/bp128.h"

#include "lib/sleef_wrapper.h"

using namespace Rcpp;
using namespace BPCells;

// [[Rcpp::export]]
IntegerVector read_integer_vector(SEXP input) {
    XPtr<UIntReader> reader(input);
    IntegerVector output(reader->size());
    for (size_t i = 0; i < reader->size(); i++) {
        output[i] = reader->read_one();
    }
    return output;
}

// [[Rcpp::export]]
SEXP open_file_reader(std::string path) {
    return Rcpp::wrap(
        XPtr<UIntReader>(new UIntReader(std::make_unique<FileUIntReader>(path.c_str()), 1024))
    );
}

// [[Rcpp::export]]
SEXP open_bp128_d1z(SEXP data, SEXP idx, SEXP idx_offsets, SEXP starts, uint32_t count) {
    XPtr<UIntReader> data_ptr(data), idx_ptr(idx), starts_ptr(starts);
    XPtr<ULongReader> idx_offsets_ptr(idx_offsets);
    return Rcpp::wrap(XPtr<UIntReader>(new UIntReader(
        std::make_unique<BP128_D1Z_UIntReader>(
            std::move(*data_ptr), std::move(*idx_ptr), std::move(*idx_offsets_ptr), std::move(*starts_ptr), count
        ),
        1024,
        1024
    )));
}

// [[Rcpp::export]]
SEXP open_bp128_d1(SEXP data, SEXP idx, SEXP idx_offsets, SEXP starts, uint32_t count) {
    XPtr<UIntReader> data_ptr(data), idx_ptr(idx), starts_ptr(starts);
    XPtr<ULongReader> idx_offsets_ptr(idx_offsets);
    return Rcpp::wrap(XPtr<UIntReader>(new UIntReader(
        std::make_unique<BP128_D1_UIntReader>(
            std::move(*data_ptr), std::move(*idx_ptr), std::move(*idx_offsets_ptr), std::move(*starts_ptr), count
        ),
        1024,
        1024
    )));
}

// [[Rcpp::export]]
SEXP open_bp128_for(SEXP data, SEXP idx, SEXP idx_offsets, uint32_t count) {
    XPtr<UIntReader> data_ptr(data), idx_ptr(idx);
    XPtr<ULongReader> idx_offsets_ptr(idx_offsets);
    return Rcpp::wrap(XPtr<UIntReader>(new UIntReader(
        std::make_unique<BP128_FOR_UIntReader>(std::move(*data_ptr), std::move(*idx_ptr), std::move(*idx_offsets_ptr), count),
        1024,
        1024
    )));
}

// [[Rcpp::export]]
std::string simd_vec_version() {
    switch (_SIMDBP128_MODE_) {
        case _SIMDBP128_X86_FULL: return "x86_full_simd";
        case _SIMDBP128_X86: return "x86_simd";
        case _SIMDBP128_ARM_NEON: return "arm_neon";
        case _SIMDBP128_C_FALLBACK: return "c_fallback";
    }
    return "Unknown";
}

// [[Rcpp::export]]
std::string simd_sleef_version() {
    switch (BPCELLS_SLEEF_MODE) {
        case _BPCELLS_SLEEF_FALLBACK: return "FALLBACK";
        case _BPCELLS_SLEEF_SSE2: return "SSE2";
        case _BPCELLS_SLEEF_AVX: return "AVX";
        case _BPCELLS_SLEEF_AVX2: return "AVX2";
        case _BPCELLS_SLEEF_NEON: return "NEON";
    }
    return "Unknown";
}

// BENCHMARKING FUNCTIONS
// These are mainly for performance benchmarking, as they only go from/to memory,
// and require pre-allocated memory buffers in order to use.
// Also, they don't report how much of the output memory buffer actually gets used which
// is inconvenient for real-world use

void copy_array_io(BulkNumReader<uint32_t> &from, BulkNumWriter<uint32_t> &to) {
    uint32_t buf[1024];
    uint32_t loaded;
    while (true) {
        loaded = from.load(buf, 1024);
        if (loaded == 0) break;
        uint32_t written = 0;
        while (written < loaded) {
            written += to.write(buf + written, loaded - written);
        }
    }
    to.finalize();
}

// [[Rcpp::export]]
void write_bp128(IntegerVector input, IntegerVector out_data, IntegerVector out_idx, NumericVector out_idx_offsets) {
    VecNumReader<uint32_t> reader((uint32_t *) &input[0], input.size());

    BP128UIntWriter writer(
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_data[0], out_data.size()),1024),
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_idx[0], out_idx.size()),1024),
        NumWriter<uint64_t>(std::make_unique<PointerNumWriter<uint64_t>>((uint64_t *) &out_idx_offsets[0], out_idx_offsets.size()),1024)
    );
    copy_array_io(reader, writer);
}

// [[Rcpp::export]]
void write_bp128_for(IntegerVector input, IntegerVector out_data, IntegerVector out_idx, NumericVector out_idx_offsets) {
    VecNumReader<uint32_t> reader((uint32_t *) &input[0], input.size());

    BP128_FOR_UIntWriter writer(
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_data[0], out_data.size()),1024),
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_idx[0], out_idx.size()),1024),
        NumWriter<uint64_t>(std::make_unique<PointerNumWriter<uint64_t>>((uint64_t *) &out_idx_offsets[0], out_idx_offsets.size()),1024)
    );
    copy_array_io(reader, writer);
}

// [[Rcpp::export]]
void write_bp128_d1(IntegerVector input, IntegerVector out_data, IntegerVector out_idx, NumericVector out_idx_offsets, IntegerVector out_starts) {
    VecNumReader<uint32_t> reader((uint32_t *) &input[0], input.size());

    BP128_D1_UIntWriter writer(
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_data[0], out_data.size()),1024),
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_idx[0], out_idx.size()),1024),
        NumWriter<uint64_t>(std::make_unique<PointerNumWriter<uint64_t>>((uint64_t *) &out_idx_offsets[0], out_idx_offsets.size()),1024),
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_starts[0], out_starts.size()), 1024)
    );
    copy_array_io(reader, writer);
}

// [[Rcpp::export]]
void write_bp128_d1z(IntegerVector input, IntegerVector out_data, IntegerVector out_idx, NumericVector out_idx_offsets, IntegerVector out_starts) {
    VecNumReader<uint32_t> reader((uint32_t *) &input[0], input.size());

    BP128_D1Z_UIntWriter writer(
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_data[0], out_data.size()),1024),
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_idx[0], out_idx.size()),1024),
        NumWriter<uint64_t>(std::make_unique<PointerNumWriter<uint64_t>>((uint64_t *) &out_idx_offsets[0], out_idx_offsets.size()),1024),
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_starts[0], out_starts.size()), 1024)
    );
    copy_array_io(reader, writer);
}

// [[Rcpp::export]]
void read_bp128(IntegerVector input_data, IntegerVector input_idx, NumericVector input_idx_offsets, IntegerVector out, double count) {
    BP128UIntReader reader(
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_data[0], input_data.size()),1024),
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_idx[0], input_idx.size()),1024),
        NumReader<uint64_t>(std::make_unique<VecNumReader<uint64_t>>((uint64_t *) &input_idx_offsets[0], input_idx_offsets.size()),1024),
        (uint64_t) count
    );

    PointerNumWriter<uint32_t> writer((uint32_t *) &out[0], out.size());

    copy_array_io(reader, writer);
}

// [[Rcpp::export]]
void read_bp128_for(IntegerVector input_data, IntegerVector input_idx, NumericVector input_idx_offsets, IntegerVector out, double count) {
    BP128_FOR_UIntReader reader(
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_data[0], input_data.size()),1024),
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_idx[0], input_idx.size()),1024),
        NumReader<uint64_t>(std::make_unique<VecNumReader<uint64_t>>((uint64_t *) &input_idx_offsets[0], input_idx_offsets.size()),1024),
        (uint64_t) count
    );

    PointerNumWriter<uint32_t> writer((uint32_t *) &out[0], out.size());

    copy_array_io(reader, writer);
}

// [[Rcpp::export]]
void read_bp128_d1(IntegerVector input_data, IntegerVector input_idx, NumericVector input_idx_offsets, IntegerVector input_starts, IntegerVector out, double count) {
    BP128_D1_UIntReader reader(
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_data[0], input_data.size()),1024),
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_idx[0], input_idx.size()),1024),
        NumReader<uint64_t>(std::make_unique<VecNumReader<uint64_t>>((uint64_t *) &input_idx_offsets[0], input_idx_offsets.size()),1024),
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_starts[0], input_starts.size()),1024),
        (uint64_t) count
    );


    PointerNumWriter<uint32_t> writer((uint32_t *) &out[0], out.size());

    copy_array_io(reader, writer);
}

// [[Rcpp::export]]
void read_bp128_d1z(IntegerVector input_data, IntegerVector input_idx, NumericVector input_idx_offsets, IntegerVector input_starts, IntegerVector out, double count) {
    BP128_D1Z_UIntReader reader(
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_data[0], input_data.size()),1024),
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_idx[0], input_idx.size()),1024),
        NumReader<uint64_t>(std::make_unique<VecNumReader<uint64_t>>((uint64_t *) &input_idx_offsets[0], input_idx_offsets.size()),1024),
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_starts[0], input_starts.size()),1024),
        (uint64_t) count
    );


    PointerNumWriter<uint32_t> writer((uint32_t *) &out[0], out.size());

    copy_array_io(reader, writer);
}

// [[Rcpp::export]]
void write_bp128_end(IntegerVector end, IntegerVector start, IntegerVector out_data, IntegerVector out_idx, NumericVector out_idx_offsets) {
    // Compress the result of end - start, mimicking the algorithm used within BPCells to store fragments
    // (i.e. do simdsubtract or simdadd on each load step)
    VecNumReader<uint32_t> reader((uint32_t *) &end[0], end.size());

    BP128UIntWriter writer(
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_data[0], out_data.size()),1024),
        NumWriter<uint32_t>(std::make_unique<PointerNumWriter<uint32_t>>((uint32_t *) &out_idx[0], out_idx.size()),1024),
        NumWriter<uint64_t>(std::make_unique<PointerNumWriter<uint64_t>>((uint64_t *) &out_idx_offsets[0], out_idx_offsets.size()),1024)
    );

    uint32_t *start_data = (uint32_t *) &start[0];

    uint32_t buf[1024];
    uint32_t loaded;
    uint64_t pos = 0;
    while (true) {
        loaded = reader.load(buf, 1024);
        if (loaded == 0) break;
        uint64_t i;
        for (i = 0; i + 128 <= loaded; i += 128) {
            simdsubtract(buf + i, start_data + pos + i);
        }
        for (; i < loaded; i++) {
            buf[i] -= start_data[pos + i];
        }

        uint32_t written = 0;
        while (written < loaded) {
            written += writer.write(buf + written, loaded - written);
        }
        pos += written;
    }
    writer.finalize();
}

// [[Rcpp::export]]
void read_bp128_end(IntegerVector input_data, IntegerVector input_idx, NumericVector input_idx_offsets, IntegerVector start, IntegerVector out, double count) {
    // Decompress the result of input + start, mimicking the algorithm used within BPCells to store fragments
    // (i.e. do simdsubtract or simdadd on each load step)
    BP128UIntReader reader(
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_data[0], input_data.size()),1024),
        NumReader<uint32_t>(std::make_unique<VecNumReader<uint32_t>>((uint32_t *) &input_idx[0], input_idx.size()),1024),
        NumReader<uint64_t>(std::make_unique<VecNumReader<uint64_t>>((uint64_t *) &input_idx_offsets[0], input_idx_offsets.size()),1024),
        (uint64_t) count
    );

    PointerNumWriter<uint32_t> writer((uint32_t *) &out[0], out.size());

    uint32_t *start_data = (uint32_t *) &start[0];

    uint32_t buf[1024];
    uint32_t loaded;
    uint64_t pos = 0;
    while (true) {
        loaded = reader.load(buf, 1024);
        if (loaded == 0) break;
        uint64_t i;
        for (i = 0; i + 128 <= loaded; i += 128) {
            simdadd(buf + i, start_data + pos + i);
        }
        for (; i < loaded; i++) {
            buf[i] += start_data[pos + i];
        }

        uint32_t written = 0;
        while (written < loaded) {
            written += writer.write(buf + written, loaded - written);
        }
        pos += written;
    }
    writer.finalize();
}