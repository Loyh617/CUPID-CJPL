/// \file src/output/RunLog.cc
/// \brief Capture console output in the active run's log while retaining terminal output.
///
/// main() owns the capture before workers start and until after shutdown.
/// Master RunAction calls BeginRun/EndRun to select each run.log. Initialization
/// and between-run context is retained in memory and copied into later logs;
/// event output is written only to its own run. RecordMacro preserves input text.

#include "output/RunLog.hh"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <streambuf>

namespace cupid {
class RunLog::State {
 public:
  class Buffer final : public std::streambuf {
   public:
    Buffer(State& state, std::streambuf* terminal) : fState(state), fTerminal(terminal) {}
   protected:
    std::streamsize xsputn(const char* data, std::streamsize size) override {
      std::lock_guard<std::mutex> lock(fState.fMutex);
      const auto written = fTerminal->sputn(data, size);
      if (fState.fFile.is_open()) fState.fFile.write(data, size);
      else fState.fContext.append(data, static_cast<std::size_t>(size));
      return written;
    }
    int_type overflow(int_type value) override {
      if (traits_type::eq_int_type(value, traits_type::eof())) return traits_type::not_eof(value);
      const char character = traits_type::to_char_type(value);
      return xsputn(&character, 1) == 1 ? value : traits_type::eof();
    }
    int sync() override {
      std::lock_guard<std::mutex> lock(fState.fMutex);
      if (fState.fFile.is_open()) fState.fFile.flush();
      return fTerminal->pubsync();
    }
   private:
    State& fState;
    std::streambuf* fTerminal;
  };
  State()
      : fCout(std::cout.rdbuf()), fCerr(std::cerr.rdbuf()), fClog(std::clog.rdbuf()),
        fOutBuffer(*this, fCout), fErrBuffer(*this, fCerr) {
    std::cout.rdbuf(&fOutBuffer);
    std::cerr.rdbuf(&fErrBuffer);
    std::clog.rdbuf(&fErrBuffer);
  }
  ~State() {
    std::cout.flush();
    std::cerr.flush();
    std::clog.flush();
    std::cout.rdbuf(fCout);
    std::cerr.rdbuf(fCerr);
    std::clog.rdbuf(fClog);
  }
  mutable std::mutex fMutex;
  std::ofstream fFile;
  std::string fContext;  // Setup context only; never buffer transport output for later runs.
  std::streambuf* fCout;
  std::streambuf* fCerr;
  std::streambuf* fClog;
  Buffer fOutBuffer;
  Buffer fErrBuffer;
};

RunLog::RunLog() : fState(std::make_unique<State>()) {}
RunLog::~RunLog() = default;

void RunLog::BeginRun(const std::string& fileName) {
  std::lock_guard<std::mutex> lock(fState->fMutex);
  if (fState->fFile.is_open()) throw std::logic_error("Previous run log is still open.");
  if (std::filesystem::exists(fileName)) throw std::runtime_error("Run log already exists: " + fileName);
  fState->fFile.open(fileName);
  if (!fState->fFile) throw std::runtime_error("Cannot create run log: " + fileName);
  fState->fFile << "CUPID_SETUP_CONTEXT_BEGIN\n" << fState->fContext << "\nCUPID_SETUP_CONTEXT_END\n";
  if (!fState->fFile) throw std::runtime_error("Cannot write run setup context.");
}

void RunLog::EndRun() {
  std::lock_guard<std::mutex> lock(fState->fMutex);
  if (!fState->fFile.is_open()) throw std::logic_error("No active run log.");
  fState->fFile.flush();
  fState->fFile.close();
  if (fState->fFile.fail()) throw std::runtime_error("Run log write failed.");
}

void RunLog::Check() const {
  std::lock_guard<std::mutex> lock(fState->fMutex);
  if (fState->fFile.is_open()) fState->fFile.flush();
  if (fState->fFile.fail()) throw std::runtime_error("Run log write failed.");
}

void RunLog::RecordMacro(const std::string& fileName, const std::vector<std::string>& searchPaths) {
  std::set<std::filesystem::path> recorded;
  std::function<void(const std::filesystem::path&)> record = [&](const auto& path) {
    const auto absolute = std::filesystem::weakly_canonical(path);
    if (!recorded.insert(absolute).second) return;
    std::ifstream file(absolute);
    if (!file) throw std::runtime_error("Cannot read run macro: " + absolute.string());
    std::ostringstream content;
    content << file.rdbuf();
    std::cout << "CUPID_MACRO_BEGIN " << absolute.string() << '\n' << content.str()
              << "\nCUPID_MACRO_END\n";
    std::istringstream lines(content.str());
    std::string line;
    while (std::getline(lines, line)) {
      std::istringstream command(line);
      std::string name, child;
      command >> name >> child;
      if (name != "/control/execute" || child.empty()) continue;
      bool found = false;
      for (const auto& directory : searchPaths) {
        const auto candidate = std::filesystem::path(directory) / child;
        if (std::filesystem::is_regular_file(candidate)) {
          record(candidate);
          found = true;
          break;
        }
      }
      if (!found) std::cout << "CUPID_MACRO_NOTE unresolved dynamic include: " << child << '\n';
    }
  };
  record(fileName);
}
}  // namespace cupid
