/// \file src/output/OutputMessenger.cc
/// \brief Bind /cupid/output/ commands to the owning RunAction's settings.
///
/// Reuse G4GenericMessenger for parameter assignment and command lifetime.
/// Settings persist between runs. Workers use the master's finalized paths
/// instead of receiving these commands or independently choosing run names.

#include "output/OutputMessenger.hh"

#include "G4GenericMessenger.hh"

namespace cupid {

OutputMessenger::OutputMessenger(G4String& fileName, G4String& outputDirectory)
    : fMessenger(std::make_unique<G4GenericMessenger>(this, "/cupid/output/", "Run output naming.")) {
  // A prefix or auto; RunAction applies recorder suffixes and collision handling.
  fMessenger->DeclareProperty("fileName", fileName,
      "Set a filename prefix (optional .root) or auto for local date/time. Persists until changed.")
      .SetStates(G4State_Idle).SetToBeBroadcasted(false)
      .SetParameterName("prefix", false);
  // A parent path, resolved relative to the process working directory at run start.
  fMessenger->DeclareProperty("directory", outputDirectory,
      "Set the parent output directory for subsequent runs; relative paths use the process working directory.")
      .SetStates(G4State_Idle).SetToBeBroadcasted(false)
      .SetParameterName("path", false);
}

OutputMessenger::~OutputMessenger() = default;

}  // namespace cupid
