// gcextract: extract a GameCube disc image (.iso/.gcm or .ciso) to a folder.
//
//   gcextract <disc.ciso|disc.iso> <out_dir>
//
// Writes sys/boot.bin, sys/bi2.bin, sys/apploader.img, sys/main.dol, sys/fst.bin
// and every file of the disc's file system under files/.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static uint32_t be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
static uint32_t le32(const uint8_t* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

// Random-access reader over a plain image or a CISO image.
class Disc {
public:
  explicit Disc(const fs::path& path) : f_(path, std::ios::binary) {
    if (!f_) throw std::runtime_error("cannot open " + path.string());
    uint8_t hdr[0x8000];
    f_.read(reinterpret_cast<char*>(hdr), sizeof hdr);
    if (f_.gcount() == sizeof hdr && std::memcmp(hdr, "CISO", 4) == 0) {
      ciso_ = true;
      block_size_ = le32(hdr + 4);
      if (block_size_ == 0) throw std::runtime_error("bad CISO block size");
      // One map byte per disc block; present blocks are stored in order after the header.
      uint32_t next = 0;
      for (size_t i = 8; i < sizeof hdr; i++) {
        map_.push_back(hdr[i] ? int64_t(next++) : -1);
      }
    }
    f_.clear();
  }

  void read(uint64_t off, void* dst, size_t len) {
    auto* out = static_cast<uint8_t*>(dst);
    if (!ciso_) {
      f_.seekg(int64_t(off));
      f_.read(reinterpret_cast<char*>(out), int64_t(len));
      if (size_t(f_.gcount()) != len) throw std::runtime_error("read past end of image");
      return;
    }
    while (len) {
      uint64_t blk = off / block_size_, in = off % block_size_;
      size_t n = size_t(std::min<uint64_t>(len, block_size_ - in));
      if (blk >= map_.size() || map_[blk] < 0) {
        std::memset(out, 0, n);  // absent block = zero fill
      } else {
        f_.seekg(int64_t(0x8000 + uint64_t(map_[blk]) * block_size_ + in));
        f_.read(reinterpret_cast<char*>(out), int64_t(n));
        if (size_t(f_.gcount()) != n) throw std::runtime_error("truncated CISO");
      }
      out += n; off += n; len -= n;
    }
  }

  std::vector<uint8_t> read(uint64_t off, size_t len) {
    std::vector<uint8_t> v(len);
    if (len) read(off, v.data(), len);
    return v;
  }

private:
  std::ifstream f_;
  bool ciso_ = false;
  uint32_t block_size_ = 0;
  std::vector<int64_t> map_;
};

static void write_file(const fs::path& p, const std::vector<uint8_t>& data) {
  fs::create_directories(p.parent_path());
  std::ofstream o(p, std::ios::binary);
  o.write(reinterpret_cast<const char*>(data.data()), int64_t(data.size()));
  if (!o) throw std::runtime_error("cannot write " + p.string());
}

static void copy_range(Disc& d, uint64_t off, uint64_t len, const fs::path& p) {
  fs::create_directories(p.parent_path());
  std::ofstream o(p, std::ios::binary);
  std::vector<uint8_t> buf(1 << 20);
  while (len) {
    size_t n = size_t(std::min<uint64_t>(len, buf.size()));
    d.read(off, buf.data(), n);
    o.write(reinterpret_cast<const char*>(buf.data()), int64_t(n));
    off += n; len -= n;
  }
  if (!o) throw std::runtime_error("cannot write " + p.string());
}

// Size of a DOL from its section table.
static uint32_t dol_size(const std::vector<uint8_t>& h) {
  uint32_t end = 0x100;
  for (int i = 0; i < 18; i++) {
    uint32_t off = be32(&h[i * 4]), sz = be32(&h[0x90 + i * 4]);
    if (off && off + sz > end) end = off + sz;
  }
  return end;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: gcextract <disc.ciso|disc.iso> <out_dir>\n");
    return 2;
  }
  try {
    Disc d{fs::path(argv[1])};
    fs::path out{argv[2]};

    auto boot = d.read(0, 0x440);
    if (be32(&boot[0x1C]) != 0xC2339F3D) throw std::runtime_error("not a GameCube disc (bad magic)");
    std::printf("game %.6s disc %u rev %u: %s\n", reinterpret_cast<char*>(boot.data()), boot[6], boot[7],
                reinterpret_cast<char*>(&boot[0x20]));

    uint32_t dol_off = be32(&boot[0x420]), fst_off = be32(&boot[0x424]), fst_len = be32(&boot[0x428]);
    write_file(out / "sys/boot.bin", boot);
    write_file(out / "sys/bi2.bin", d.read(0x440, 0x2000));
    auto apl = d.read(0x2440, 0x20);
    write_file(out / "sys/apploader.img", d.read(0x2440, 0x20 + be32(&apl[0x14]) + be32(&apl[0x18])));
    auto dolh = d.read(dol_off, 0x100);
    write_file(out / "sys/main.dol", d.read(dol_off, dol_size(dolh)));
    auto fst = d.read(fst_off, fst_len);
    write_file(out / "sys/fst.bin", fst);

    // FST: 12-byte entries, entry 0 is the root whose length is the entry count; names follow.
    uint32_t count = be32(&fst[8]);
    if (uint64_t(count) * 12 > fst.size()) throw std::runtime_error("bad FST");
    const char* names = reinterpret_cast<const char*>(&fst[count * 12]);
    std::vector<std::pair<uint32_t, fs::path>> dirs{{count, out / "files"}};
    fs::create_directories(out / "files");
    uint64_t files = 0, bytes = 0;
    for (uint32_t i = 1; i < count; i++) {
      while (i >= dirs.back().first) dirs.pop_back();
      const uint8_t* e = &fst[i * 12];
      fs::path p = dirs.back().second / fs::u8path(names + (be32(e) & 0xFFFFFF));
      if (e[0]) {  // directory: field 2 = index one past its last entry
        fs::create_directories(p);
        dirs.push_back({be32(e + 8), p});
      } else {
        copy_range(d, be32(e + 4), be32(e + 8), p);
        files++; bytes += be32(e + 8);
      }
    }
    std::printf("extracted %llu files, %.1f MB\n", (unsigned long long)files, bytes / 1048576.0);
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "gcextract: %s\n", ex.what());
    return 1;
  }
  return 0;
}
