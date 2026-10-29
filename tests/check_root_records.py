"""Read actual simulation ROOT files and compare event and incident energy totals.

Requires PyROOT from the ROOT installation (source thisroot.sh first).
"""
import argparse
import collections
import json
import math
import pathlib

import ROOT

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('files', nargs='+', type=pathlib.Path)
parser.add_argument('--events', type=int, required=True)
parser.add_argument('--detectors', nargs='+', default=['LMO0', 'LMO1'])
parser.add_argument('--run', type=int, default=0)
parser.add_argument('--report', type=pathlib.Path)
parser.add_argument('--event-policy', choices=['positive', 'all'], default='positive',
                    help='Use all for older files that retain zero-energy event rows.')
parser.add_argument('--allow-empty-files', action='store_true',
                    help='Allow metadata-only idle-worker files; verify zero assigned events in the run log separately.')
args = parser.parse_args()

def detector_name(tree):
    leaf = tree.GetLeaf('DetectorName')
    if leaf and leaf.InheritsFrom('TLeafC'):
        return str(leaf.GetValueString())
    return str(tree.DetectorName)


events = {}
incident_totals = collections.defaultdict(float)
incident_keys = set()
incident_rows = 0
file_counts = []
for path in args.files:
    file = ROOT.TFile.Open(str(path), 'READ')
    assert file and not file.IsZombie() and not file.TestBit(ROOT.TFile.kRecovered), f'Invalid ROOT file: {path}'
    schema = file.Get('SchemaVersion')
    format_version = 1
    if schema:
        assert schema.GetTitle() == '1' and file.Get('EnergyUnit').GetTitle() == 'keV', 'Unexpected legacy schema'
    else:
        for name in ['EventEnergy', 'IncidentEnergy']:
            tree = file.Get(name)
            if tree:
                title = str(tree.GetTitle())
                version = 3 if 'Edep in keV; CUPID schema 3' in title else 2
                assert f'Edep in keV; CUPID schema {version}' in title, 'Unexpected native schema'
                assert format_version in (1, version), 'Mixed table schemas'
                format_version = version
    event_tree = file.Get('EventEnergy')
    incident_tree = file.Get('IncidentEnergy')
    if schema and not event_tree and not incident_tree and args.allow_empty_files:
        file_counts.append({'file': str(path.resolve()), 'event_rows': 0, 'incident_rows': 0,
                            'metadata_only': True})
        file.Close()
        continue
    assert event_tree and incident_tree, f'Missing requested tree: {path}'
    file_counts.append({'file': str(path.resolve()), 'event_rows': int(event_tree.GetEntries()),
                        'incident_rows': int(incident_tree.GetEntries())})
    if format_version == 3:
        assert [b.GetName() for b in incident_tree.GetListOfBranches()] == [
            'RunID', 'EventID', 'TrackID', 'PDG', 'Edep', 'DetectorName'], 'Unexpected incident columns'
        assert incident_tree.GetLeaf('PDG').GetTypeName() == 'Int_t', 'Unexpected PDG type'
    for row in event_tree:
        key = (int(row.RunID), int(row.EventID), detector_name(row))
        energy = float(row.Edep)
        assert key not in events, f'Duplicate event/detector row: {key}'
        assert math.isfinite(energy) and energy >= 0., f'Invalid energy: {key}'
        if args.event_policy == 'positive':
            assert energy > 0., f'Zero-energy event row: {key}'
        events[key] = energy
    for row in incident_tree:
        key = (int(row.RunID), int(row.EventID), detector_name(row))
        root_key = (*key, int(row.TrackID))
        energy = float(row.Edep)
        assert row.TrackID > 0 and root_key not in incident_keys, f'Invalid/duplicate incident key: {root_key}'
        assert math.isfinite(energy) and energy >= 0., f'Invalid energy: {root_key}'
        if format_version == 3:
            assert energy > 0., f'Zero-energy incident row: {root_key}'
            pdg = int(row.PDG)  # Preserve signed, zero and nuclear Geant4 encodings.
            assert -(2**31) <= pdg < 2**31, f'Invalid PDG field: {root_key}'
        incident_keys.add(root_key)
        incident_totals[key] += energy
        incident_rows += 1
    file.Close()

# The requested event count comes from the caller/run log, not from sparse rows.
assert args.events >= 0 and len(set(args.detectors)) == len(args.detectors), 'Invalid expected run layout'
keys = set(events) | set(incident_totals)
for run_id, event_id, detector in keys:
    assert run_id == args.run and 0 <= event_id < args.events and detector in args.detectors, f'Unexpected record key: {(run_id, event_id, detector)}'
if args.event_policy == 'all':
    assert len(events) == args.events * len(args.detectors), 'Missing event/detector rows'
else:
    positive_incident_keys = {key for key, energy in incident_totals.items() if energy > 0.}
    assert set(events) == positive_incident_keys, 'Positive event and incident records disagree'
max_difference = 0.
for key in keys:
    energy = events.get(key, 0.)
    incident_energy = incident_totals.get(key, 0.)
    difference = abs(energy - incident_energy)
    max_difference = max(max_difference, difference)
    assert math.isclose(energy, incident_energy, rel_tol=1.e-10, abs_tol=1.e-8), f'Energy mismatch: {key}'

summary = {'result': 'PASS', 'run_id': args.run, 'events': args.events, 'energy_unit': 'keV',
           'event_rows': len(events), 'incident_rows': incident_rows,
           'event_row_policy': args.event_policy, 'complete_event_coverage_verified': args.event_policy == 'all',
           'max_event_incident_difference_keV': max_difference, 'files': file_counts, 'detectors': {}}
for detector in args.detectors:
    energies = [value for key, value in events.items() if key[2] == detector]
    summary['detectors'][detector] = {'event_rows': len(energies), 'positive_edep_events': sum(e > 0 for e in energies),
                                    'sum_edep_keV': math.fsum(energies), 'max_edep_keV': max(energies, default=0.)}
text = json.dumps(summary, indent=2, ensure_ascii=False)
if args.report:
    args.report.write_text(text + '\n')
print(text)
