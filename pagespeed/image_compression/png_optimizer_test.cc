/**
 * Copyright 2009 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: Bryan McQuade

#include <fstream>
#include <sstream>
#include <string>

#include "base/basictypes.h"
#include "pagespeed/image_compression/gif_reader.h"
#include "pagespeed/image_compression/png_optimizer.h"
#include "pagespeed/image_compression/read_image.h"
#include "pagespeed/image_compression/scanline_utils.h"
#include "pagespeed/testing/pagespeed_test.h"
#include "third_party/libpng/png.h"

namespace {

using pagespeed::image_compression::GifReader;
using pagespeed::image_compression::PngOptimizer;
using pagespeed::image_compression::PngReader;
using pagespeed::image_compression::PngReaderInterface;
using pagespeed::image_compression::PngScanlineReaderRaw;
using pagespeed::image_compression::PngScanlineReader;
using pagespeed::image_compression::ScopedPngStruct;

// The *_TEST_DIR_PATH macros are set by the gyp target that builds this file.
const char kGifTestDir[] = IMAGE_TEST_DIR_PATH "gif/";
const char kPngSuiteTestDir[] = IMAGE_TEST_DIR_PATH "pngsuite/";
const char kPngSuiteGifTestDir[] = IMAGE_TEST_DIR_PATH "pngsuite/gif/";
const char kPngTestDir[] = IMAGE_TEST_DIR_PATH "png/";

void ReadImageToString(const std::string& dir,
                       const char* file_name,
                       const char* ext,
                       std::string* dest) {
  const std::string path = dir + file_name + '.' + ext;
  ASSERT_TRUE(pagespeed_testing::ReadFileToString(path, dest));
}

void ReadPngSuiteFileToString(const char* file_name, std::string* dest) {
  ReadImageToString(kPngSuiteTestDir, file_name, "png", dest);
}

// Structure that holds metadata and actual pixel data for a decoded
// PNG.
struct ReadPngDescriptor {
  unsigned char* img_bytes;  // The actual pixel data.
  unsigned char* img_rgba_bytes; // RGBA data for the image.
  unsigned long width;
  unsigned long height;
  int channels;              // 3 for RGB, 4 for RGB+alpha
  unsigned long row_bytes;   // number of bytes in a row
  unsigned char bg_red, bg_green, bg_blue;
  bool bgcolor_retval;

  ReadPngDescriptor() : img_bytes(NULL), img_rgba_bytes(NULL) {}
  ~ReadPngDescriptor() {
    free(img_bytes);
    if (channels != 4) {
      free(img_rgba_bytes);
    }
  }
};

// Decode the PNG and store the decoded data and metadata in the
// ReadPngDescriptor struct.
void PopulateDescriptor(const std::string& img,
                        ReadPngDescriptor* desc,
                        const char* identifier) {
  PngScanlineReader scanline_reader;
  scanline_reader.set_transform(
      // Expand paletted colors into true RGB triplets
      // Expand grayscale images to full 8 bits from 1, 2, or 4 bits/pixel
      // Expand paletted or RGB images with transparency to full alpha
      // channels so the data will be available as RGBA quartets.
      PNG_TRANSFORM_EXPAND |
      // Downsample images with 16bits per channel to 8bits per channel.
      PNG_TRANSFORM_STRIP_16 |
      // Convert grayscale images to RGB images.
      PNG_TRANSFORM_GRAY_TO_RGB);

  if (setjmp(*scanline_reader.GetJmpBuf())) {
    FAIL();
  }
  PngReader reader;
  if (!scanline_reader.InitializeRead(reader, img)) {
    FAIL();
  }

  int channels;
  switch (scanline_reader.GetPixelFormat()) {
    case pagespeed::image_compression::RGB_888:
      channels = 3;
      break;
    case pagespeed::image_compression::RGBA_8888:
      channels = 4;
      break;
    case pagespeed::image_compression::GRAY_8:
      channels = 1;
      break;
    default:
      LOG(INFO) << "Unexpected pixel format: "
                << scanline_reader.GetPixelFormat();
      channels = -1;
      break;
  }

  desc->width = scanline_reader.GetImageWidth();
  desc->height = scanline_reader.GetImageHeight();
  desc->channels = channels;
  desc->row_bytes = scanline_reader.GetBytesPerScanline();
  desc->img_bytes = static_cast<unsigned char*>(
      malloc(desc->row_bytes * desc->height));
  if (channels == 4) {
    desc->img_rgba_bytes = desc->img_bytes;
  } else {
    desc->img_rgba_bytes = static_cast<unsigned char*>(
        malloc(4 * desc->width * desc->height));
  }
  desc->bgcolor_retval = scanline_reader.GetBackgroundColor(
      &desc->bg_red, &desc->bg_green, &desc->bg_blue);

  unsigned char* next_scanline_to_write = desc->img_bytes;
  while (scanline_reader.HasMoreScanLines()) {
    void* scanline = NULL;
    if (scanline_reader.ReadNextScanline(&scanline)) {
      memcpy(next_scanline_to_write, scanline,
             scanline_reader.GetBytesPerScanline());
      next_scanline_to_write += scanline_reader.GetBytesPerScanline();
    }
  }
  if (channels != 4) {
    memset(desc->img_rgba_bytes, 0xff, 4 * desc->height * desc->width);
    unsigned char* next_img_byte = desc->img_bytes;
    unsigned char* next_rgba_byte = desc->img_rgba_bytes;
    for (unsigned long pixel_count = 0;
         pixel_count < desc->height * desc->width;
         ++pixel_count) {
      if (channels == 3) {
        memcpy(next_rgba_byte, next_img_byte, channels);
      } else if (channels == 1) {
        memset(next_rgba_byte, *next_img_byte, 3);
      } else {
        FAIL() << "Unexpected number of channels: " << channels;
      }
      next_img_byte += channels;
      next_rgba_byte += 4;
    }
  }
}


void AssertPngEq(
    const std::string& orig, const std::string& opt, const char* identifier,
    const std::string& in_rgba) {
  // Gather data and metadata for the original and optimized PNGs.
  ReadPngDescriptor orig_desc;
  PopulateDescriptor(orig, &orig_desc, identifier);
  ReadPngDescriptor opt_desc;
  PopulateDescriptor(opt, &opt_desc, identifier);

  // Verify that the dimensions match.
  EXPECT_EQ(orig_desc.width, opt_desc.width)
      << "width mismatch for " << identifier;
  EXPECT_EQ(orig_desc.height, opt_desc.height)
      << "height mismatch for " << identifier;

  // If PNG background chunks are supported, verify that the
  // background chunks are not present in the optimized image.
#if defined(PNG_bKGD_SUPPORTED) || defined(PNG_READ_BACKGROUND_SUPPORTED)
  EXPECT_FALSE(opt_desc.bgcolor_retval) << "Unexpected: bgcolor";
#endif

  // Verify that the number of channels matches (should be 3 for RGB
  // or 4 for RGB+alpha)
  EXPECT_EQ(orig_desc.channels, opt_desc.channels)
      << "channel mismatch for " << identifier;

  // Verify that the number of bytes in a row matches.
  EXPECT_EQ(orig_desc.row_bytes, opt_desc.row_bytes)
      << "row_bytes mismatch for " << identifier;

  // Verify that the actual image data matches.
  if (orig_desc.row_bytes == opt_desc.row_bytes &&
      orig_desc.height == opt_desc.height) {
    const unsigned long img_bytes_size = orig_desc.row_bytes * orig_desc.height;
    EXPECT_EQ(0,
              memcmp(orig_desc.img_bytes, opt_desc.img_bytes, img_bytes_size))
        << "image data mismatch for " << identifier;
  }

  if (!in_rgba.empty()) {
    unsigned long num_rgba_bytes = 4 * opt_desc.height * opt_desc.width;
    EXPECT_EQ(in_rgba.length(), num_rgba_bytes)
        << "rgba data size mismatch for " << identifier;
    EXPECT_EQ(0,
              memcmp(opt_desc.img_rgba_bytes, in_rgba.c_str(), num_rgba_bytes))
        << "rgba data bytes mismatch for " << identifier;
  }
}


struct ImageCompressionInfo {
  const char* filename;
  size_t original_size;
  size_t compressed_size_best;
  size_t compressed_size_default;
  int width;
  int height;
  int original_bit_depth;
  int original_color_type;
  int compressed_bit_depth;
  int compressed_color_type;
};

// These images were obtained from
// http://www.libpng.org/pub/png/pngsuite.html
ImageCompressionInfo kValidImages[] = {
  { "basi0g01", 217, 208, 217, 32, 32, 1, 0, 1, 0 },
  { "basi0g02", 154, 154, 154, 32, 32, 2, 0, 2, 0 },
  { "basi0g04", 247, 145, 247, 32, 32, 4, 0, 4, 0 },
  { "basi0g08", 254, 250, 799, 32, 32, 8, 0, 8, 0 },
  { "basi0g16", 299, 285, 1223, 32, 32, 16, 0, 16, 0 },
  { "basi2c08", 315, 313, 1509, 32, 32, 8, 2, 8, 2 },
  { "basi2c16", 595, 557, 2863, 32, 32, 16, 2, 16, 2 },
  { "basi3p01", 132, 132, 132, 32, 32, 1, 3, 1, 3 },
  { "basi3p02", 193, 178, 178, 32, 32, 2, 3, 2, 3 },
  { "basi3p04", 327, 312, 312, 32, 32, 4, 3, 4, 3 },
  { "basi3p08", 1527, 1518, 1527, 32, 32, 8, 3, 8, 3 },
  { "basi4a08", 214, 209, 1450, 32, 32, 8, 4, 8, 4 },
  { "basi4a16", 2855, 1980, 1980, 32, 32, 16, 4, 16, 4 },
  { "basi6a08", 361, 350, 1591, 32, 32, 8, 6, 8, 6 },
  { "basi6a16", 4180, 4133, 4423, 32, 32, 16, 6, 16, 6 },
  { "basn0g01", 164, 164, 164, 32, 32, 1, 0, 1, 0 },
  { "basn0g02", 104, 104, 104, 32, 32, 2, 0, 2, 0 },
  { "basn0g04", 145, 103, 145, 32, 32, 4, 0, 4, 0 },
  { "basn0g08", 138, 132, 730, 32, 32, 8, 0, 8, 0 },
  { "basn0g16", 167, 152, 645, 32, 32, 16, 0, 16, 0 },
  { "basn2c08", 145, 145, 1441, 32, 32, 8, 2, 8, 2 },
  { "basn2c16", 302, 274, 2687, 32, 32, 16, 2, 16, 2 },
  { "basn3p01", 112, 112, 112, 32, 32, 1, 3, 1, 3 },
  { "basn3p02", 146, 131, 131, 32, 32, 2, 3, 2, 3 },
  { "basn3p04", 216, 201, 201, 32, 32, 4, 3, 4, 3 },
  { "basn3p08", 1286, 1286, 1286, 32, 32, 8, 3, 8, 3 },
  { "basn4a08", 126, 121, 1433, 32, 32, 8, 4, 8, 4 },
  { "basn4a16", 2206, 1185, 1185, 32, 32, 16, 4, 16, 4 },
  { "basn6a08", 184, 176, 1435, 32, 32, 8, 6, 8, 6 },
  { "basn6a16", 3435, 3271, 4181, 32, 32, 16, 6, 16, 6 },
  { "bgai4a08", 214, 209, 1450, 32, 32, 8, 4, 8, 4 },
  { "bgai4a16", 2855, 1980, 1980, 32, 32, 16, 4, 16, 4 },
  { "bgan6a08", 184, 176, 1435, 32, 32, 8, 6, 8, 6 },
  { "bgan6a16", 3435, 3271, 4181, 32, 32, 16, 6, 16, 6 },
  { "bgbn4a08", 140, 121, 1433, 32, 32, 8, 4, 8, 4 },
  { "bggn4a16", 2220, 1185, 1185, 32, 32, 16, 4, 16, 4 },
  { "bgwn6a08", 202, 176, 1435, 32, 32, 8, 6, 8, 6 },
  { "bgyn6a16", 3453, 3271, 4181, 32, 32, 16, 6, 16, 6 },
  { "ccwn2c08", 1514, 1456, 1742, 32, 32, 8, 2, 8, 2 },
  { "ccwn3p08", 1554, 1499, 1510, 32, 32, 8, 3, 8, 3 },
  { "cdfn2c08", 404, 498, 532,8, 32, 8, 2, 8, 3 },
  { "cdhn2c08", 344, 476, 491, 32, 8, 8, 2, 8, 3 },
  { "cdsn2c08", 232, 255, 258,8, 8, 8, 2, 8, 3 },
  { "cdun2c08", 724, 928, 942, 32, 32, 8, 2, 8, 3 },
  { "ch1n3p04", 258, 201, 201, 32, 32, 4, 3, 4, 3 },
  { "ch2n3p08", 1810, 1286, 1286, 32, 32, 8, 3, 8, 3 },
  { "cm0n0g04", 292, 271, 273, 32, 32, 4, 0, 4, 0 },
  { "cm7n0g04", 292, 271, 273, 32, 32, 4, 0, 4, 0 },
  { "cm9n0g04", 292, 271, 273, 32, 32, 4, 0, 4, 0 },
  { "cs3n2c16", 214, 178, 216, 32, 32, 16, 2, 16, 2 },
  { "cs3n3p08", 259, 244, 244, 32, 32, 8, 3, 8, 3 },
  { "cs5n2c08", 186, 226, 256, 32, 32, 8, 2, 8, 3 },
  { "cs5n3p08", 271, 256, 256, 32, 32, 8, 3, 8, 3 },
  { "cs8n2c08", 149, 226, 256, 32, 32, 8, 2, 8, 3 },
  { "cs8n3p08", 256, 256, 256, 32, 32, 8, 3, 8, 3 },
  { "ct0n0g04", 273, 271, 273, 32, 32, 4, 0, 4, 0 },
  { "ct1n0g04", 792, 271, 273, 32, 32, 4, 0, 4, 0 },
  { "ctzn0g04", 753, 271, 273, 32, 32, 4, 0, 4, 0 },
  { "f00n0g08", 319, 312, 319, 32, 32, 8, 0, 8, 0 },
  { "f00n2c08", 2475, 1070, 2475, 32, 32, 8, 2, 8, 2 },
  { "f01n0g08", 321, 246, 283, 32, 32, 8, 0, 8, 0 },
  { "f01n2c08", 1180, 965, 2546, 32, 32, 8, 2, 8, 2 },
  { "f02n0g08", 355, 289, 297, 32, 32, 8, 0, 8, 0 },
  { "f02n2c08", 1729, 1024, 2512, 32, 32, 8, 2, 8, 2 },
  { "f03n0g08", 389, 292, 296, 32, 32, 8, 0, 8, 0 },
  { "f03n2c08", 1291, 1062, 2509, 32, 32, 8, 2, 8, 2 },
  { "f04n0g08", 269, 273, 281, 32, 32, 8, 0, 8, 0 },
  { "f04n2c08", 985, 985, 2546, 32, 32, 8, 2, 8, 2 },
  { "g03n0g16", 345, 273, 308, 32, 32, 16, 0, 8, 0 },
  { "g03n2c08", 370, 396, 490, 32, 32, 8, 2, 8, 3 },
  { "g03n3p04", 214, 214, 214, 32, 32, 4, 3, 4, 3 },
  { "g04n0g16", 363, 287, 310, 32, 32, 16, 0, 8, 0 },
  { "g04n2c08", 377, 399, 493, 32, 32, 8, 2, 8, 3 },
  { "g04n3p04", 219, 219, 219, 32, 32, 4, 3, 4, 3 },
  { "g05n0g16", 339, 275, 306, 32, 32, 16, 0, 8, 0 },
  { "g05n2c08", 350, 402, 488, 32, 32, 8, 2, 8, 3 },
  { "g05n3p04", 206, 206, 206, 32, 32, 4, 3, 4, 3 },
  { "g07n0g16", 321, 261, 305, 32, 32, 16, 0, 8, 0 },
  { "g07n2c08", 340, 401, 488, 32, 32, 8, 2, 8, 3 },
  { "g07n3p04", 207, 207, 207, 32, 32, 4, 3, 4, 3 },
  { "g10n0g16", 262, 210, 306, 32, 32, 16, 0, 8, 0 },
  { "g10n2c08", 285, 403, 495, 32, 32, 8, 2, 8, 3 },
  { "g10n3p04", 214, 214, 214, 32, 32, 4, 3, 4, 3 },
  { "g25n0g16", 383, 305, 305, 32, 32, 16, 0, 8, 0 },
  { "g25n2c08", 405, 399, 470, 32, 32, 8, 2, 8, 3 },
  { "g25n3p04", 215, 215, 215, 32, 32, 4, 3, 4, 3 },
  { "oi1n0g16", 167, 152, 645, 32, 32, 16, 0, 16, 0 },
  { "oi1n2c16", 302, 274, 2687, 32, 32, 16, 2, 16, 2 },
  { "oi2n0g16", 179, 152, 645, 32, 32, 16, 0, 16, 0 },
  { "oi2n2c16", 314, 274, 2687, 32, 32, 16, 2, 16, 2 },
  { "oi4n0g16", 203, 152, 645, 32, 32, 16, 0, 16, 0 },
  { "oi4n2c16", 338, 274, 2687, 32, 32, 16, 2, 16, 2 },
  { "oi9n0g16", 1283, 152, 645, 32, 32, 16, 0, 16, 0 },
  { "oi9n2c16", 3038, 274, 2687, 32, 32, 16, 2, 16, 2 },
  { "pp0n2c16", 962, 274, 2687, 32, 32, 16, 2, 16, 2 },
  { "pp0n6a08", 818, 158, 3006, 32, 32, 8, 6, 8, 6 },
  { "ps1n0g08", 1477, 132, 730, 32, 32, 8, 0, 8, 0 },
  { "ps1n2c16", 1641, 274, 2687, 32, 32, 16, 2, 16, 2 },
  { "ps2n0g08", 2341, 132, 730, 32, 32, 8, 0, 8, 0 },
  { "ps2n2c16", 2505, 274, 2687, 32, 32, 16, 2, 16, 2 },
  { "s01i3p01", 113, 98, 98, 1, 1, 1, 3, 1, 3 },
  { "s01n3p01", 113, 98, 98, 1, 1, 1, 3, 1, 3 },
  { "s02i3p01", 114, 100, 100, 2, 2, 1, 3, 1, 3 },
  { "s02n3p01", 115, 100, 100, 2, 2, 1, 3, 1, 3 },
  { "s03i3p01", 118, 103, 103, 3, 3, 1, 3, 1, 3 },
  { "s03n3p01", 120, 105, 105, 3, 3, 1, 3, 1, 3 },
  { "s04i3p01", 126, 111, 111, 4, 4, 1, 3, 1, 3 },
  { "s04n3p01", 121, 106, 106, 4, 4, 1, 3, 1, 3 },
  { "s05i3p02", 134, 119, 119, 5, 5, 2, 3, 2, 3 },
  { "s05n3p02", 129, 114, 114, 5, 5, 2, 3, 2, 3 },
  { "s06i3p02", 143, 128, 128, 6, 6, 2, 3, 2, 3 },
  { "s06n3p02", 131, 116, 116, 6, 6, 2, 3, 2, 3 },
  { "s07i3p02", 149, 134, 134, 7, 7, 2, 3, 2, 3 },
  { "s07n3p02", 138, 123, 123, 7, 7, 2, 3, 2, 3 },
  { "s08i3p02", 149, 134, 134, 8, 8, 2, 3, 2, 3 },
  { "s08n3p02", 139, 124, 124, 8, 8, 2, 3, 2, 3 },
  { "s09i3p02", 147, 132, 132, 9, 9, 2, 3, 2, 3 },
  { "s09n3p02", 143, 130, 130, 9, 9, 2, 3, 2, 3 },
  { "s32i3p04", 355, 340, 340, 32, 32, 4, 3, 4, 3 },
  { "s32n3p04", 263, 248, 248, 32, 32, 4, 3, 4, 3 },
  { "s33i3p04", 385, 370, 370, 33, 33, 4, 3, 4, 3 },
  { "s33n3p04", 329, 314, 314, 33, 33, 4, 3, 4, 3 },
  { "s34i3p04", 349, 332, 334, 34, 34, 4, 3, 4, 3 },
  { "s34n3p04", 248, 229, 233, 34, 34, 4, 3, 4, 3 },
  { "s35i3p04", 399, 384, 384, 35, 35, 4, 3, 4, 3 },
  { "s35n3p04", 338, 313, 323, 35, 35, 4, 3, 4, 3 },
  { "s36i3p04", 356, 339, 341, 36, 36, 4, 3, 4, 3 },
  { "s36n3p04", 258, 240, 243, 36, 36, 4, 3, 4, 3 },
  { "s37i3p04", 393, 378, 378, 37, 37, 4, 3, 4, 3 },
  { "s37n3p04", 336, 317, 321, 37, 37, 4, 3, 4, 3 },
  { "s38i3p04", 357, 339, 342, 38, 38, 4, 3, 4, 3 },
  { "s38n3p04", 245, 228, 230, 38, 38, 4, 3, 4, 3 },
  { "s39i3p04", 420, 405, 405, 39, 39, 4, 3, 4, 3 },
  { "s39n3p04", 352, 336, 337, 39, 39, 4, 3, 4, 3 },
  { "s40i3p04", 357, 340, 342, 40, 40, 4, 3, 4, 3 },
  { "s40n3p04", 256, 237, 241, 40, 40, 4, 3, 4, 3 },
  { "tbbn1g04", 419, 405, 405, 32, 32, 4, 0, 4, 0 },
  { "tbbn2c16", 1994, 1095, 1113, 32, 32, 16, 2, 8, 3 },
  { "tbbn3p08", 1128, 1095, 1115, 32, 32, 8, 3, 8, 3 },
  { "tbgn2c16", 1994, 1095, 1113, 32, 32, 16, 2, 8, 3 },
  { "tbgn3p08", 1128, 1095, 1115, 32, 32, 8, 3, 8, 3 },
  { "tbrn2c08", 1347, 1095, 1113, 32, 32, 8, 2, 8, 3 },
  { "tbwn1g16", 1146, 582, 599, 32, 32, 16, 0, 8, 0 },
  { "tbwn3p08", 1131, 1095, 1115, 32, 32, 8, 3, 8, 3 },
  { "tbyn3p08", 1131, 1095, 1115, 32, 32, 8, 3, 8, 3 },
  { "tp0n1g08", 689, 568, 585, 32, 32, 8, 0, 8, 0 },
  { "tp0n2c08", 1311, 1099, 1119, 32, 32, 8, 2, 8, 3 },
  { "tp0n3p08", 1120, 1098, 1120, 32, 32, 8, 3, 8, 3 },
  { "tp1n3p08", 1115, 1095, 1115, 32, 32, 8, 3, 8, 3 },
  { "z00n2c08", 3172, 224, 1956, 32, 32, 8, 2, 8, 2 },
  { "z03n2c08", 232, 224, 1956, 32, 32, 8, 2, 8, 2 },
  { "z06n2c08", 224, 224, 1956, 32, 32, 8, 2, 8, 2 },
  { "z09n2c08", 224, 224, 1956, 32, 32, 8, 2, 8, 2 },

};

ImageCompressionInfo kValidGifImages[] = {
  { "basi0g01", 153, 166, 166, 32, 32, 8, 3, 1, 3 },
  { "basi0g02", 185, 112, 112, 32, 32, 8, 3, 2, 3 },
  { "basi0g04", 344, 144, 186, 32, 32, 8, 3, 4, 3 },
  { "basi0g08", 1736, 116, 714, 32, 32, 8, 3, 8, 0 },
  { "basi3p01", 138, 96, 96, 32, 32, 8, 3, 1, 3 },
  { "basi3p02", 186, 115, 115, 32, 32, 8, 3, 2, 3 },
  { "basi3p04", 344, 185, 185, 32, 32, 8, 3, 4, 3 },
  { "basi3p08", 1737, 1270, 1270, 32, 32, 8, 3, 8, 3 },
  { "basn0g01", 153, 166, 166, 32, 32, 8, 3, 1, 3 },
  { "basn0g02", 185, 112, 112, 32, 32, 8, 3, 2, 3 },
  { "basn0g04", 344, 144, 186, 32, 32, 8, 3, 4, 3 },
  { "basn0g08", 1736, 116, 714, 32, 32, 8, 3, 8, 0 },
  { "basn3p01", 138, 96, 96, 32, 32, 8, 3, 1, 3 },
  { "basn3p02", 186, 115, 115, 32, 32, 8, 3, 2, 3 },
  { "basn3p04", 344, 185, 185, 32, 32, 8, 3, 4, 3 },
  { "basn3p08", 1737, 1270, 1270, 32, 32, 8, 3, 8, 3 },

  // These files have been transformed by rounding the original png
  // 8-bit alpha channel into a 1-bit alpha channel for gif.
  { "tr-basi4a08", 467, 239, 316, 32, 32, 8, 3, 8, 3 },
  { "tr-basn4a08", 467, 239, 316, 32, 32, 8, 3, 8, 3 },
};

const char* kInvalidFiles[] = {
  "emptyfile",
  "x00n0g01",
  "xcrn0g04",
  "xlfn0g04",
};

struct OpaqueImageInfo {
  const char* filename;
  bool is_opaque;
  int in_color_type;
  int out_color_type;
};

OpaqueImageInfo kOpaqueImagesWithAlpha[] = {
  { "rgba_opaque", 1, 6, 2 },
  { "grey_alpha_opaque", 1, 4, 0 },
  { "bgai4a16", 0, 4, 4 }
};

#define WRITE_OPTIMIZED_IMAGES 0
void WriteStringToFile(const std::string &file_name, std::string &src) {
#if WRITE_OPTIMIZED_IMAGES
  const std::string path = kPngTestDir + file_name;
  std::ofstream stream;
  stream.open(path.c_str(), std::ofstream::out | std::ofstream::binary);
  stream.write(src.c_str(), src.size());
  stream.close();
#endif
}

void AssertMatch(const std::string& in,
                 const std::string& ref,
                 PngReaderInterface* reader,
                 const ImageCompressionInfo& info,
                 std::string in_rgba) {
  PngReader png_reader;
  int width, height, bit_depth, color_type;
  std::string out;
  EXPECT_EQ(info.original_size, in.size())
      << info.filename;
  ASSERT_TRUE(reader->GetAttributes(
      in, &width, &height, &bit_depth, &color_type)) << info.filename;
  EXPECT_EQ(info.width, width) << info.filename;
  EXPECT_EQ(info.height, height) << info.filename;
  EXPECT_EQ(info.original_bit_depth, bit_depth) << info.filename;
  EXPECT_EQ(info.original_color_type, color_type) << info.filename;

  ASSERT_TRUE(PngOptimizer::OptimizePng(*reader, in, &out)) << info.filename;
  EXPECT_EQ(info.compressed_size_default, out.size()) << info.filename;
  AssertPngEq(ref, out, info.filename, in_rgba);

  ASSERT_TRUE(png_reader.GetAttributes(
      out, &width, &height, &bit_depth, &color_type)) << info.filename;
  EXPECT_EQ(info.compressed_bit_depth, bit_depth) << info.filename;
  EXPECT_EQ(info.compressed_color_type, color_type) << info.filename;

  ASSERT_TRUE(PngOptimizer::OptimizePngBestCompression(*reader, in, &out))
      << info.filename;
  EXPECT_EQ(info.compressed_size_best, out.size()) << info.filename;
  AssertPngEq(ref, out, info.filename, in_rgba);

  ASSERT_TRUE(png_reader.GetAttributes(
      out, &width, &height, &bit_depth, &color_type)) << info.filename;
  EXPECT_EQ(info.compressed_bit_depth, bit_depth) << info.filename;
  EXPECT_EQ(info.compressed_color_type, color_type) << info.filename;
  WriteStringToFile(std::string("z") + info.filename, out);
}

void AssertMatch(const std::string& in,
                 const std::string& ref,
                 PngReaderInterface* reader,
                 const ImageCompressionInfo& info) {
  static std::string in_rgba;
  AssertMatch(in, ref, reader, info, in_rgba);
}

bool InitializeEntireReader(const std::string& image_string,
                            const PngReader& png_reader,
                            PngScanlineReader* entire_image_reader) {
  // Initialize entire_image_reader.
  if (!entire_image_reader->Reset()) {
    return false;
  }
  entire_image_reader->set_transform(
    PNG_TRANSFORM_EXPAND |
    PNG_TRANSFORM_STRIP_16);

  if (entire_image_reader->InitializeRead(png_reader, image_string) == false) {
    return false;
  }

  // Skip the images which are not supported by PngScanlineReader.
  return (entire_image_reader->GetPixelFormat()
          != pagespeed::image_compression::UNSUPPORTED);
}

const size_t kValidImageCount = arraysize(kValidImages);
const size_t kValidGifImageCount = arraysize(kValidGifImages);
const size_t kInvalidFileCount = arraysize(kInvalidFiles);
const size_t kOpaqueImagesWithAlphaCount = arraysize(kOpaqueImagesWithAlpha);

TEST(PngOptimizerTest, ValidPngs) {
  PngReader reader;
  for (size_t i = 0; i < kValidImageCount; i++) {
    std::string in, out;
    ReadPngSuiteFileToString(kValidImages[i].filename, &in);
    AssertMatch(in, in, &reader, kValidImages[i]);
  }
}

TEST(PngScanlineReaderTest, InitializeRead_validPngs) {
  PngScanlineReader scanline_reader;
  if (setjmp(*scanline_reader.GetJmpBuf())) {
    ASSERT_FALSE(true) << "Execution should never reach here";
  }
  for (size_t i = 0; i < kValidImageCount; i++) {
    std::string in, out;
    ReadPngSuiteFileToString(kValidImages[i].filename, &in);
    PngReader png_reader;
    ASSERT_TRUE(scanline_reader.Reset());

    int width, height, bit_depth, color_type;
    ASSERT_TRUE(png_reader.GetAttributes(
        in, &width, &height, &bit_depth, &color_type));

    EXPECT_EQ(kValidImages[i].original_color_type, color_type);
    ASSERT_TRUE(scanline_reader.InitializeRead(png_reader, in));
    EXPECT_EQ(kValidImages[i].original_color_type,
              scanline_reader.GetColorType());
  }

  for (size_t i = 0; i < kOpaqueImagesWithAlphaCount; i++) {
    std::string in, out;
    ReadPngSuiteFileToString(kOpaqueImagesWithAlpha[i].filename, &in);
    PngReader png_reader;
    ASSERT_TRUE(scanline_reader.Reset());

    int width, height, bit_depth, color_type;
    ASSERT_TRUE(png_reader.GetAttributes(
        in, &width, &height, &bit_depth, &color_type));

    EXPECT_EQ(kOpaqueImagesWithAlpha[i].in_color_type, color_type);
    ASSERT_TRUE(scanline_reader.InitializeRead(png_reader, in));
    EXPECT_EQ(kOpaqueImagesWithAlpha[i].out_color_type,
              scanline_reader.GetColorType());
  }
}

TEST(PngOptimizerTest, ValidPngs_isOpaque) {
  ScopedPngStruct read(ScopedPngStruct::READ);

  for (size_t i = 0; i < kOpaqueImagesWithAlphaCount; i++) {
    std::string in, out;
    ReadPngSuiteFileToString(kOpaqueImagesWithAlpha[i].filename, &in);
    scoped_ptr<PngReaderInterface> png_reader(new PngReader);
    ASSERT_TRUE(png_reader->ReadPng(in, read.png_ptr(), read.info_ptr(), 0));
    EXPECT_EQ(kOpaqueImagesWithAlpha[i].is_opaque,
              PngReaderInterface::IsAlphaChannelOpaque(
                  read.png_ptr(), read.info_ptr()));
    ASSERT_TRUE(read.reset());
  }
}

TEST(PngOptimizerTest, LargerPng) {
  PngReader reader;
  std::string in, out;
  ReadImageToString(kPngTestDir, "this_is_a_test", "png", &in);
  ASSERT_EQ(static_cast<size_t>(20316), in.length());
  ASSERT_TRUE(PngOptimizer::OptimizePng(reader, in, &out));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader.GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(640, width);
  EXPECT_EQ(400, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(2, color_type);

  ASSERT_TRUE(reader.GetAttributes(
      out, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(640, width);
  EXPECT_EQ(400, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(0, color_type);
}

TEST(PngOptimizerTest, InvalidPngs) {
  PngReader reader;
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    std::string in, out;
    ReadPngSuiteFileToString(kInvalidFiles[i], &in);
    ASSERT_FALSE(PngOptimizer::OptimizePngBestCompression(reader, in, &out));
    ASSERT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));

    int width, height, bit_depth, color_type;
    const bool get_attributes_result = reader.GetAttributes(
        in, &width, &height, &bit_depth, &color_type);
    bool expected_get_attributes_result = false;
    if (strcmp("x00n0g01", kInvalidFiles[i]) == 0) {
      // Special case: even though the image is invalid, it has a
      // valid IDAT chunk, so we can read its attributes.
      expected_get_attributes_result = true;
    }
    EXPECT_EQ(expected_get_attributes_result, get_attributes_result)
        << kInvalidFiles[i];
  }
}

TEST(PngOptimizerTest, FixPngOutOfBoundReadCrash) {
  PngReader reader;
  std::string in, out;
  ReadImageToString(kPngTestDir, "read_from_stream_crash", "png", &in);
  ASSERT_EQ(static_cast<size_t>(193), in.length());
  ASSERT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader.GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(32, width);
  EXPECT_EQ(32, height);
  EXPECT_EQ(2, bit_depth);
  EXPECT_EQ(3, color_type);
}

TEST(PngOptimizerTest, PartialPng) {
  PngReader reader;
  std::string in, out;
  int width, height, bit_depth, color_type;
  ReadImageToString(kPngTestDir, "pagespeed-128", "png", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  // Loop, removing the last byte repeatedly to generate every
  // possible partial version of the animated PNG.
  while (true) {
    if (in.size() == 0) {
      break;
    }
    // Remove the last byte.
    in.erase(in.length() - 1);
    EXPECT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));

    // See if we can extract image attributes. Doing so requires that
    // at least 33 bytes are available (signature plus full IDAT
    // chunk).
    bool png_header_available = (in.size() >= 33);
    bool get_attributes_result =
        reader.GetAttributes(in, &width, &height, &bit_depth, &color_type);
    EXPECT_EQ(png_header_available, get_attributes_result) << in.size();
    if (get_attributes_result) {
      EXPECT_EQ(128, width);
      EXPECT_EQ(128, height);
      EXPECT_EQ(8, bit_depth);
      EXPECT_EQ(3, color_type);
    }
  }
}

TEST(PngOptimizerTest, ValidGifs) {
  GifReader reader;
  for (size_t i = 0; i < kValidGifImageCount; i++) {
    std::string in, ref, gif_rgba;
    ReadImageToString(
        kPngSuiteGifTestDir, kValidGifImages[i].filename, "gif", &in);
    ReadImageToString(
        kPngSuiteGifTestDir, kValidGifImages[i].filename, "gif.rgba",
        &gif_rgba);
    ReadPngSuiteFileToString(kValidGifImages[i].filename, &ref);
    AssertMatch(in, ref, &reader, kValidGifImages[i], gif_rgba);
  }
}

TEST(PngOptimizerTest, AnimatedGif) {
  GifReader reader;
  std::string in, out;
  ReadImageToString(kGifTestDir, "animated", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader.GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(120, width);
  EXPECT_EQ(50, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(3, color_type);
}

TEST(PngOptimizerTest, InterlacedGif) {
  GifReader reader;
  std::string in, out;
  ReadImageToString(kGifTestDir, "interlaced", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_TRUE(PngOptimizer::OptimizePng(reader, in, &out));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader.GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(213, width);
  EXPECT_EQ(323, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(3, color_type);
}

TEST(PngOptimizerTest, TransparentGif) {
  GifReader reader;
  std::string in, out;
  ReadImageToString(kGifTestDir, "transparent", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_TRUE(PngOptimizer::OptimizePng(reader, in, &out));

  int width, height, bit_depth, color_type;
  ASSERT_TRUE(reader.GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
  EXPECT_EQ(320, width);
  EXPECT_EQ(320, height);
  EXPECT_EQ(8, bit_depth);
  EXPECT_EQ(3, color_type);
}

// Verify that we fail gracefully when processing partial versions of
// the animated GIF.
TEST(PngOptimizerTest, PartialAnimatedGif) {
  GifReader reader;
  std::string in, out;
  int width, height, bit_depth, color_type;
  ReadImageToString(kGifTestDir, "animated", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  // Loop, removing the last byte repeatedly to generate every
  // possible partial version of the animated gif.
  while (true) {
    if (in.size() == 0) {
      break;
    }
    // Remove the last byte.
    in.erase(in.length() - 1);
    EXPECT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));

    // See if we can extract image attributes. Doing so requires that
    // at least 10 bytes are available.
    bool gif_header_available = (in.size() >= 10);
    bool get_attributes_result =
        reader.GetAttributes(in, &width, &height, &bit_depth, &color_type);
    EXPECT_EQ(gif_header_available, get_attributes_result) << in.size();
    if (get_attributes_result) {
      EXPECT_EQ(120, width);
      EXPECT_EQ(50, height);
      EXPECT_EQ(8, bit_depth);
      EXPECT_EQ(3, color_type);
    }
  }
}

// Make sure we do not leak memory when attempting to optimize a GIF
// that fails to decode.
TEST(PngOptimizerTest, BadGifNoLeak) {
  GifReader reader;
  std::string in, out;
  ReadImageToString(kGifTestDir, "bad", "gif", &in);
  ASSERT_NE(static_cast<size_t>(0), in.length());
  ASSERT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));
  int width, height, bit_depth, color_type;
  ASSERT_FALSE(reader.GetAttributes(
      in, &width, &height, &bit_depth, &color_type));
}

TEST(PngOptimizerTest, InvalidGifs) {
  // Verify that we fail gracefully when trying to parse PNGs using
  // the GIF reader.
  GifReader reader;
  for (size_t i = 0; i < kValidImageCount; i++) {
    std::string in, out;
    ReadPngSuiteFileToString(kValidImages[i].filename, &in);
    ASSERT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));
    int width, height, bit_depth, color_type;
    ASSERT_FALSE(reader.GetAttributes(
        in, &width, &height, &bit_depth, &color_type));
  }

  // Also verify we fail gracefully for the invalid PNG images.
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    std::string in, out;
    ReadPngSuiteFileToString(kInvalidFiles[i], &in);
    ASSERT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));
    int width, height, bit_depth, color_type;
    ASSERT_FALSE(reader.GetAttributes(
        in, &width, &height, &bit_depth, &color_type));
  }
}

// Make sure that after we fail, we're still able to successfully
// compress valid images.
TEST(PngOptimizerTest, SuccessAfterFailure) {
  PngReader reader;
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    {
      std::string in, out;
      ReadPngSuiteFileToString(kInvalidFiles[i], &in);
      ASSERT_FALSE(PngOptimizer::OptimizePng(reader, in, &out));
    }

    {
      std::string in, out;
      ReadPngSuiteFileToString(kValidImages[i].filename, &in);
      ASSERT_TRUE(PngOptimizer::OptimizePng(reader, in, &out));
      int width, height, bit_depth, color_type;
      ASSERT_TRUE(reader.GetAttributes(
          in, &width, &height, &bit_depth, &color_type));
    }
  }
}

TEST(PngOptimizerTest, ScopedPngStruct) {
  ScopedPngStruct read(ScopedPngStruct::READ);
  ASSERT_TRUE(read.valid());
  ASSERT_NE(static_cast<png_structp>(NULL), read.png_ptr());
  ASSERT_NE(static_cast<png_infop>(NULL), read.info_ptr());

  ScopedPngStruct write(ScopedPngStruct::WRITE);
  ASSERT_TRUE(write.valid());
  ASSERT_NE(static_cast<png_structp>(NULL), write.png_ptr());
  ASSERT_NE(static_cast<png_infop>(NULL), write.info_ptr());

#ifdef NDEBUG
  ScopedPngStruct invalid(static_cast<ScopedPngStruct::Type>(-1));
  ASSERT_FALSE(invalid.valid());
  ASSERT_EQ(static_cast<png_structp>(NULL), invalid.png_ptr());
  ASSERT_EQ(static_cast<png_infop>(NULL), invalid.info_ptr());
#else
  ASSERT_DEATH(ScopedPngStruct t =
               ScopedPngStruct(static_cast<ScopedPngStruct::Type>(-1)),
               "Invalid Type");
#endif
}

TEST(PngReaderTest, ReadTransparentPng) {
  ScopedPngStruct read(ScopedPngStruct::READ);
  PngReader reader;
  std::string in;
  ReadPngSuiteFileToString("basn4a16", &in);
  // Don't require_opaque.
  ASSERT_TRUE(reader.ReadPng(in, read.png_ptr(), read.info_ptr(),
                             PNG_TRANSFORM_IDENTITY, false));
  ASSERT_FALSE(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr()));
  ASSERT_TRUE(read.reset());

  // Don't transform but require opaque.
  ASSERT_FALSE(reader.ReadPng(in, read.png_ptr(), read.info_ptr(),
                              PNG_TRANSFORM_IDENTITY, true));
  ASSERT_TRUE(read.reset());

  // Strip the alpha channel and require opaque.
  ASSERT_TRUE(reader.ReadPng(in, read.png_ptr(), read.info_ptr(),
                             PNG_TRANSFORM_STRIP_ALPHA, true));
#ifndef NDEBUG
  ASSERT_DEATH(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr()),
               "IsAlphaChannelOpaque called for image without alpha channel.");
#else
  ASSERT_FALSE(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr()));
#endif
  ASSERT_TRUE(read.reset());

  // Strip the alpha channel and don't require opaque.
  ASSERT_TRUE(reader.ReadPng(in, read.png_ptr(), read.info_ptr(),
                             PNG_TRANSFORM_STRIP_ALPHA, false));
#ifndef NDEBUG
  ASSERT_DEATH(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr()),
               "IsAlphaChannelOpaque called for image without alpha channel.");
#else
  ASSERT_FALSE(reader.IsAlphaChannelOpaque(read.png_ptr(), read.info_ptr()));
#endif
  ASSERT_TRUE(read.reset());

}

TEST(PngScanlineReaderRawTest, ValidPngsRow) {
  // Create a reader which tries to read a row of image at a time.
  PngScanlineReaderRaw per_row_reader;

  // Create a reader which reads the entire image.
  PngReader png_reader;
  PngScanlineReader entire_image_reader;
  if (setjmp(*entire_image_reader.GetJmpBuf()) != 0) {
    FAIL();
  }

  for (size_t i = 0; i < kValidImageCount; i++) {
    std::string image_string;
    ReadPngSuiteFileToString(kValidImages[i].filename, &image_string);

    // Initialize entire_image_reader (PngScanlineReader).
    if (!InitializeEntireReader(image_string, png_reader,
                                &entire_image_reader)) {
      // This image is not supported by PngScanlineReader. Skip it.
      continue;
    }

    // Initialize per_row_reader.
    ASSERT_TRUE(per_row_reader.Initialize(image_string.data(),
                                          image_string.length()));

    // Make sure the images sizes and the pixel formats are the same.
    ASSERT_EQ(entire_image_reader.GetImageWidth(),
              per_row_reader.GetImageWidth());
    ASSERT_EQ(entire_image_reader.GetImageHeight(),
              per_row_reader.GetImageHeight());
    ASSERT_EQ(entire_image_reader.GetPixelFormat(),
              per_row_reader.GetPixelFormat());

    const int width = per_row_reader.GetImageWidth();
    const int num_channels =
      GetNumChannelsFromPixelFormat(per_row_reader.GetPixelFormat());
    uint8* buffer_per_row = NULL;
    uint8* buffer_entire = NULL;

    // Decode and check the image a row at a time.
    while (per_row_reader.HasMoreScanLines() &&
           entire_image_reader.HasMoreScanLines()) {
      ASSERT_TRUE(entire_image_reader.ReadNextScanline(
        reinterpret_cast<void**>(&buffer_entire)));

      ASSERT_TRUE(per_row_reader.ReadNextScanline(
        reinterpret_cast<void**>(&buffer_per_row)));

      for (int i = 0; i < width*num_channels; ++i) {
        ASSERT_EQ(buffer_entire[i], buffer_per_row[i]);
      }
    }

    // Make sure both readers have exhausted all image rows.
    ASSERT_FALSE(per_row_reader.HasMoreScanLines());
    ASSERT_FALSE(entire_image_reader.HasMoreScanLines());
  }
}

TEST(PngScanlineReaderRawTest, ValidPngsEntire) {
  PngReader png_reader;
  PngScanlineReader entire_image_reader;
  if (setjmp(*entire_image_reader.GetJmpBuf()) != 0) {
    FAIL();
  }

  for (size_t i = 0; i < kValidImageCount; ++i) {
    std::string image_string;
    ReadPngSuiteFileToString(kValidImages[i].filename, &image_string);

    // Initialize entire_image_reader (PngScanlineReader).
    if (!InitializeEntireReader(image_string, png_reader,
                                &entire_image_reader)) {
      // This image is not supported by PngScanlineReader. Skip it.
      continue;
    }

    size_t width, bytes_per_row;
    void* buffer_for_raw_reader;
    pagespeed::image_compression::PixelFormat pixel_format;
    ASSERT_TRUE(ReadImage(pagespeed::image_compression::IMAGE_PNG,
                          image_string.data(), image_string.length(),
                          &buffer_for_raw_reader, &pixel_format, &width, NULL,
                          &bytes_per_row));
    uint8* buffer_per_row = static_cast<uint8*>(buffer_for_raw_reader);
    int num_channels = GetNumChannelsFromPixelFormat(pixel_format);

    // Check the image row by row.
    while (entire_image_reader.HasMoreScanLines()) {
      uint8* buffer_entire = NULL;
      ASSERT_TRUE(entire_image_reader.ReadNextScanline(
        reinterpret_cast<void**>(&buffer_entire)));

      for (size_t i = 0; i < width*num_channels; ++i) {
        ASSERT_EQ(buffer_entire[i], buffer_per_row[i]);
      }
      buffer_per_row += bytes_per_row;
    }

    free(buffer_for_raw_reader);
  }
}

TEST(PngScanlineReaderRawTest, PartialRead) {
  uint8* buffer = NULL;
  std::string image_string;
  ReadPngSuiteFileToString(kValidImages[0].filename, &image_string);

  // Initialize a reader but do not read any scanline.
  PngScanlineReaderRaw reader1;
  ASSERT_TRUE(reader1.Initialize(image_string.data(), image_string.length()));

  // Initialize a reader and read one scanline.
  PngScanlineReaderRaw reader2;
  ASSERT_TRUE(reader2.Initialize(image_string.data(), image_string.length()));
  ASSERT_TRUE(reader2.ReadNextScanline(reinterpret_cast<void**>(&buffer)));

  // Initialize a reader, and try to read a scanline after the image has been
  // depleted.
  PngScanlineReaderRaw reader3;
  ASSERT_TRUE(reader3.Initialize(image_string.data(), image_string.length()));
  while (reader3.HasMoreScanLines()) {
    ASSERT_TRUE(reader3.ReadNextScanline(reinterpret_cast<void**>(&buffer)));
  }
  ASSERT_FALSE(reader3.ReadNextScanline(reinterpret_cast<void**>(&buffer)));
}

TEST(PngScanlineReaderRawTest, ReadAfterReset) {
  uint8* buffer = NULL;
  std::string image_string;
  ReadPngSuiteFileToString(kValidImages[0].filename, &image_string);

  // Initialize a reader and read one scanline.
  PngScanlineReaderRaw reader;
  ASSERT_TRUE(reader.Initialize(image_string.data(), image_string.length()));
  ASSERT_TRUE(reader.ReadNextScanline(reinterpret_cast<void**>(&buffer)));
  // Now re-initialize the reader.
  ASSERT_TRUE(reader.Initialize(image_string.data(), image_string.length()));

  // Decode the entire image using ReadImage(). This copy of image will be used
  // as the baseline.
  size_t width, bytes_per_row;
  void* buffer_for_raw_reader;
  pagespeed::image_compression::PixelFormat pixel_format;
  ASSERT_TRUE(ReadImage(pagespeed::image_compression::IMAGE_PNG,
                        image_string.data(), image_string.length(),
                        &buffer_for_raw_reader, &pixel_format, &width, NULL,
                        &bytes_per_row));
  uint8* buffer_entire = static_cast<uint8*>(buffer_for_raw_reader);
  int num_channels = GetNumChannelsFromPixelFormat(pixel_format);

  // Compare the image row by row.
  while (reader.HasMoreScanLines()) {
    uint8* buffer_row = NULL;
    ASSERT_TRUE(reader.ReadNextScanline(
      reinterpret_cast<void**>(&buffer_row)));

    for (size_t i = 0; i < width*num_channels; ++i) {
      ASSERT_EQ(buffer_entire[i], buffer_row[i]);
    }
    buffer_entire += bytes_per_row;
  }

  free(buffer_for_raw_reader);
}

TEST(PngScanlineReaderRawTest, InvalidPngs) {
  PngScanlineReaderRaw reader;
  for (size_t i = 0; i < kInvalidFileCount; i++) {
    std::string image_string;
    ReadPngSuiteFileToString(kInvalidFiles[i], &image_string);

    ASSERT_FALSE(reader.Initialize(image_string.data(), image_string.length()));

    ASSERT_FALSE(ReadImage(pagespeed::image_compression::IMAGE_PNG,
                           image_string.data(), image_string.length(),
                           NULL, NULL, NULL, NULL, NULL));
  }
}
}  // namespace
