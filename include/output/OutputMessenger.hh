/// \file include/output/OutputMessenger.hh
/// \brief Register output filename and directory commands for RunAction.
///
/// RunAction creates this messenger only on the master or in serial mode.
/// The bound strings must outlive it. Commands update settings in Idle;
/// RunAction validates them and prepares the output directory at run start.

#pragma once

#include "globals.hh"

#include <memory>

class G4GenericMessenger;

namespace cupid {

class OutputMessenger final {
 public:
  OutputMessenger(G4String& fileName, G4String& outputDirectory);
  ~OutputMessenger();

 private:
  std::unique_ptr<G4GenericMessenger> fMessenger;
};

}  // namespace cupid
