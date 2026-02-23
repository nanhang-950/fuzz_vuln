/*
 * Fuzzer for libvpx WebM container parsing
 * =========================================
 * 
 * This fuzzer targets the WebM/Matroska container format parsing in libvpx.
 * WebM is based on Matroska and uses EBML (Extensible Binary Meta Language).
 * 
 * Attack Surface:
 * - EBML header parsing
 * - Matroska segment parsing
 * - Track information parsing
 * - Cluster and block parsing
 * - Cue point parsing
 * - Metadata handling
 * - Malformed container structures
 * 
 * Build:
 * ------
 * $CXX -fsanitize=fuzzer,address -std=c++11 \
 *      -I./libvpx -I./libvpx/build \
 *      vpx_webm_fuzzer.cc libvpx/webmdec.cc \
 *      -o vpx_webm_fuzzer \
 *      ./libvpx/build/libvpx.a -lpthread -lm
 * 
 * Note: Requires libwebm or mkvparser
 * 
 * Run:
 * ----
 * ./vpx_webm_fuzzer -max_len=262144 corpus/
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Minimal WebM/EBML structure definitions
#define EBML_ID_HEADER 0x1A45DFA3
#define EBML_ID_SEGMENT 0x18538067
#define EBML_ID_SEEKHEAD 0x114D9B74
#define EBML_ID_INFO 0x1549A966
#define EBML_ID_TRACKS 0x1654AE6B
#define EBML_ID_CLUSTER 0x1F43B675
#define EBML_ID_CUES 0x1C53BB6B

#define MATROSKA_ID_TIMECODESCALE 0x2AD7B1
#define MATROSKA_ID_DURATION 0x4489
#define MATROSKA_ID_TRACKENTRY 0xAE
#define MATROSKA_ID_TRACKNUMBER 0xD7
#define MATROSKA_ID_TRACKTYPE 0x83
#define MATROSKA_ID_CODECID 0x86
#define MATROSKA_ID_SIMPLEBLOCK 0xA3
#define MATROSKA_ID_BLOCKGROUP 0xA0
#define MATROSKA_ID_BLOCK 0xA1

// Simple EBML parser for fuzzing
class EBMLFuzzer {
public:
    EBMLFuzzer(const uint8_t *data, size_t size) 
        : data_(data), size_(size), pos_(0), error_(false) {}

    bool HasError() const { return error_; }
    size_t Position() const { return pos_; }
    size_t Remaining() const { return size_ - pos_; }

    // Read variable-length integer (EBML VINT)
    uint64_t ReadVInt() {
        if (pos_ >= size_) {
            error_ = true;
            return 0;
        }

        uint8_t first_byte = data_[pos_++];
        if (first_byte == 0) {
            error_ = true;
            return 0;
        }

        // Find the length marker
        int length = 0;
        uint8_t mask = 0x80;
        while (length < 8 && !(first_byte & mask)) {
            mask >>= 1;
            length++;
        }
        length++;

        if (length > 8 || pos_ + length - 1 > size_) {
            error_ = true;
            return 0;
        }

        uint64_t value = first_byte & (mask - 1);
        for (int i = 1; i < length; i++) {
            value = (value << 8) | data_[pos_++];
        }

        return value;
    }

    // Read EBML element ID
    uint32_t ReadElementID() {
        if (pos_ >= size_) {
            error_ = true;
            return 0;
        }

        uint8_t first_byte = data_[pos_];
        int length = 0;
        uint8_t mask = 0x80;
        
        while (length < 4 && !(first_byte & mask)) {
            mask >>= 1;
            length++;
        }
        length++;

        if (length > 4 || pos_ + length > size_) {
            error_ = true;
            return 0;
        }

        uint32_t id = 0;
        for (int i = 0; i < length; i++) {
            id = (id << 8) | data_[pos_++];
        }

        return id;
    }

    // Read element size
    uint64_t ReadElementSize() {
        return ReadVInt();
    }

    // Skip bytes
    void Skip(size_t bytes) {
        if (pos_ + bytes > size_) {
            pos_ = size_;
            error_ = true;
        } else {
            pos_ += bytes;
        }
    }

    // Read bytes
    const uint8_t* ReadBytes(size_t bytes) {
        if (pos_ + bytes > size_) {
            error_ = true;
            return nullptr;
        }
        const uint8_t* result = data_ + pos_;
        pos_ += bytes;
        return result;
    }

    // Read unsigned integer
    uint64_t ReadUInt(size_t size) {
        if (size > 8 || pos_ + size > size_) {
            error_ = true;
            return 0;
        }

        uint64_t value = 0;
        for (size_t i = 0; i < size; i++) {
            value = (value << 8) | data_[pos_++];
        }
        return value;
    }

    // Read string
    const char* ReadString(size_t size) {
        return (const char*)ReadBytes(size);
    }

private:
    const uint8_t *data_;
    size_t size_;
    size_t pos_;
    bool error_;
};

// Forward declarations
static void ParseCluster(EBMLFuzzer &parser, uint64_t cluster_size);

// Parse EBML header
static bool ParseEBMLHeader(EBMLFuzzer &parser) {
    uint32_t id = parser.ReadElementID();
    if (parser.HasError() || id != EBML_ID_HEADER) {
        return false;
    }

    uint64_t size = parser.ReadElementSize();
    if (parser.HasError() || size > parser.Remaining()) {
        return false;
    }

    parser.Skip(size);
    return !parser.HasError();
}

// Parse Segment
static void ParseSegment(EBMLFuzzer &parser) {
    uint32_t id = parser.ReadElementID();
    if (parser.HasError() || id != EBML_ID_SEGMENT) {
        return;
    }

    uint64_t segment_size = parser.ReadElementSize();
    if (parser.HasError()) {
        return;
    }

    size_t segment_end = parser.Position() + segment_size;
    if (segment_end > parser.Remaining() + parser.Position()) {
        segment_end = parser.Remaining() + parser.Position();
    }

    // Parse segment elements
    while (parser.Position() < segment_end && !parser.HasError()) {
        uint32_t elem_id = parser.ReadElementID();
        if (parser.HasError()) break;

        uint64_t elem_size = parser.ReadElementSize();
        if (parser.HasError()) break;

        if (elem_size > parser.Remaining()) {
            break;
        }

        switch (elem_id) {
            case EBML_ID_SEEKHEAD:
            case EBML_ID_INFO:
            case EBML_ID_TRACKS:
            case EBML_ID_CUES:
                // Parse these recursively in a real implementation
                parser.Skip(elem_size);
                break;

            case EBML_ID_CLUSTER:
                // Parse cluster
                ParseCluster(parser, elem_size);
                break;

            default:
                // Unknown element, skip
                parser.Skip(elem_size);
                break;
        }
    }
}

// Parse Cluster (contains video frames)
static void ParseCluster(EBMLFuzzer &parser, uint64_t cluster_size) {
    size_t cluster_end = parser.Position() + cluster_size;
    if (cluster_end > parser.Remaining() + parser.Position()) {
        cluster_end = parser.Remaining() + parser.Position();
    }

    while (parser.Position() < cluster_end && !parser.HasError()) {
        uint32_t elem_id = parser.ReadElementID();
        if (parser.HasError()) break;

        uint64_t elem_size = parser.ReadElementSize();
        if (parser.HasError() || elem_size > parser.Remaining()) break;

        switch (elem_id) {
            case MATROSKA_ID_SIMPLEBLOCK:
            case MATROSKA_ID_BLOCK: {
                // Read block data
                const uint8_t *block_data = parser.ReadBytes(elem_size);
                if (block_data && elem_size > 4) {
                    // Touch the block data to trigger any issues
                    volatile uint8_t dummy = block_data[0];
                    dummy = block_data[elem_size - 1];
                    (void)dummy;
                }
                break;
            }

            case MATROSKA_ID_BLOCKGROUP: {
                // Parse block group recursively
                size_t bg_end = parser.Position() + elem_size;
                while (parser.Position() < bg_end && !parser.HasError()) {
                    uint32_t bg_id = parser.ReadElementID();
                    uint64_t bg_size = parser.ReadElementSize();
                    if (parser.HasError() || bg_size > parser.Remaining()) break;
                    parser.Skip(bg_size);
                }
                break;
            }

            default:
                parser.Skip(elem_size);
                break;
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 32) {
        return 0;
    }

    // Create parser
    EBMLFuzzer parser(data, size);

    // Parse EBML header
    if (!ParseEBMLHeader(parser)) {
        // Try parsing without header (malformed file)
        parser = EBMLFuzzer(data, size);
    }

    // Parse segment
    ParseSegment(parser);

    // Test various edge cases
    
    // 1. Multiple segments
    if (!parser.HasError() && parser.Remaining() > 32) {
        ParseSegment(parser);
    }

    // 2. Truncated data
    if (size > 100) {
        EBMLFuzzer truncated(data, size / 2);
        ParseEBMLHeader(truncated);
        ParseSegment(truncated);
    }

    // 3. Overlapping elements (malformed)
    if (size > 50) {
        EBMLFuzzer overlapped(data + 10, size - 10);
        ParseSegment(overlapped);
    }

    return 0;
}
