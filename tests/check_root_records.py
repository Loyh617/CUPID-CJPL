"""Read actual simulation ROOT files and compare event and incident energy totals.

Pass both recorder files for schema 4; historical combined schema 1/2/3 files
remain supported. Requires PyROOT from the ROOT installation (source thisroot.sh first).
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
seen_types = set()
for path in args.files:
    file = ROOT.TFile.Open(str(path), 'READ')
    assert file and not file.IsZombie() and not file.TestBit(ROOT.TFile.kRecovered), f'Invalid ROOT file: {path}'
    schema = file.Get('SchemaVersion')
    if schema:
        assert schema.GetTitle() == '1' and file.Get('EnergyUnit').GetTitle() == 'keV', 'Unexpected legacy schema'
    tables = []
    file_types = set()
    versions = set()
    tree_names = set()
    for entry in file.GetListOfKeys():
        if entry.GetClassName() != 'TTree':
            continue
        name = str(entry.GetName())
        assert name not in tree_names, f'Duplicate tree cycle: {path}/{name}'
        tree_names.add(name)
        tree = file.Get(name)
        title = str(tree.GetTitle())
        if 'CUPID schema 4' in title:
            kind = title.split(';')[0]
            assert kind in ('EventEnergy', 'IncidentEnergy'), 'Unknown record type'
            assert title == f'{kind}; Edep in keV; CUPID schema 4', 'Unexpected units/schema'
            assert name in args.detectors, f'Unexpected detector tree: {name}'
            version = 4
            detector = name
        else:
            kind = name
            assert kind in ('EventEnergy', 'IncidentEnergy'), f'Unknown legacy tree: {name}'
            version = 1 if schema else (3 if 'CUPID schema 3' in title else 2)
            if version > 1:
                assert f'Edep in keV; CUPID schema {version}' in title, 'Unexpected native schema'
            detector = None
        columns = ['RunID', 'EventID']
        if kind == 'IncidentEnergy' and version >= 3:
            columns += ['TrackID', 'PDG']
        columns += ['Edep']
        if version < 4:
            columns += ['DetectorName']
        if kind == 'IncidentEnergy' and version < 3:
            columns += ['TrackID']
        assert [b.GetName() for b in tree.GetListOfBranches()] == columns, f'Unexpected columns: {path}/{name}'
        if version >= 3 and kind == 'IncidentEnergy':
            assert tree.GetLeaf('PDG').GetTypeName() == 'Int_t', 'Unexpected PDG type'
        versions.add(version)
        file_types.add(kind)
        seen_types.add(kind)
        tables.append((tree, kind, version, detector))
    if not tables:
        assert schema and args.allow_empty_files, f'No data trees: {path}'
    assert len(versions) <= 1, 'Mixed schemas in one file'
    if 4 in versions:
        assert len(file_types) == 1, 'Different recorders in the same schema-4 file'
        assert tree_names == set(args.detectors), 'Incomplete detector catalog'
    elif tables:
        assert file_types == {'EventEnergy', 'IncidentEnergy'}, f'Missing legacy tree: {path}'
    counts = {'file': str(path.resolve()), 'event_rows': 0, 'incident_rows': 0}
    if not tables:
        counts['metadata_only'] = True
    for tree, kind, version, detector in tables:
        counts['event_rows' if kind == 'EventEnergy' else 'incident_rows'] += int(tree.GetEntries())
        for row in tree:
            key = (int(row.RunID), int(row.EventID), detector if detector is not None else detector_name(row))
            energy = float(row.Edep)
            assert math.isfinite(energy) and energy >= 0., f'Invalid energy: {key}'
            if kind == 'EventEnergy':
                assert key not in events, f'Duplicate event/detector row: {key}'
                if args.event_policy == 'positive':
                    assert energy > 0., f'Zero-energy event row: {key}'
                events[key] = energy
            else:
                root_key = (*key, int(row.TrackID))
                assert row.TrackID > 0 and root_key not in incident_keys, f'Invalid/duplicate incident key: {root_key}'
                if version >= 3:
                    assert energy > 0., f'Zero-energy incident row: {root_key}'
                    pdg = int(row.PDG)
                    assert -(2**31) <= pdg < 2**31, f'Invalid PDG field: {root_key}'
                incident_keys.add(root_key)
                incident_totals[key] += energy
                incident_rows += 1
    file_counts.append(counts)
    file.Close()

assert seen_types == {'EventEnergy', 'IncidentEnergy'} or (args.allow_empty_files and not seen_types), \
    'Energy comparison requires both recorder files'

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
