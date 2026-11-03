"""Check production Action merging, zero rows, idle workers, empty files, repeated runs and consolidated logs."""
import hashlib
import pathlib
import re
import subprocess
import sys
import tempfile

executable = pathlib.Path(sys.argv[1]).resolve()
checker = pathlib.Path(sys.argv[2]).resolve()
base = pathlib.Path(sys.argv[3]).resolve()
base.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=base) as temporary:
    directory = pathlib.Path(temporary)
    geometry = directory / 'geometry.mac'
    geometry.write_text('/cupid/geometry/enable LMOArray\n/cupid/geometry/lmoCount 2\n')
    macro = directory / 'test.mac'
    macro.write_text("""/control/verbose 0
/random/setSeeds 12567 76521
/control/execute geometry.mac
/run/initialize
/gun/position -25 -350 0 mm
/gun/direction 0 0 1
/gun/particle e-
/gun/energy 100 keV
/run/beamOn 32
/gun/particle geantino
/gun/energy 1 MeV
/run/beamOn 32
/gun/particle e-
/gun/energy 100 keV
/gun/position 25 -350 0 mm
/cupid/output/fileName calibration.root
/run/beamOn 1
/gun/particle geantino
/gun/position 0 1000 0 mm
/cupid/output/fileName auto
/run/beamOn 3
""")
    for mode, options in [('serial', ['--serial']), ('mt', ['--threads', '4'])]:
        output = directory / mode
        command = [str(executable), str(macro), *options, '--output', str(output)]
        result = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'Analysis_W' not in result.stdout + result.stderr, result.stdout + result.stderr
        log = (output / 'run.log').read_text()
        names = {int(run): name for run, name in re.findall(r'CUPID_OUTPUT_NAME run=(\d+) requested=\S+ base=(\S+) clock=local', log)}
        assert set(names) == {0, 1, 2, 3}, names
        for run in (0, 1, 3):
            assert re.fullmatch(r'run\d{8}_\d{6}(?:_[1-9]\d*)?\.root', names[run]), names
        assert len(set(names.values())) == 4, 'Repeated runs reused a resolved filename'
        assert names[2] == 'calibration.root', names
        expected_files = {'run.log'}
        for run in (0, 2):
            expected_files.update(names[run][:-5] + suffix for suffix in ('_EventEnergy.root', '_IncidentEnergy.root'))
        assert {p.name for p in output.iterdir()} == expected_files

        for expected in ['CUPID_OUTPUT_MODULE name=RootOutput',
                         'CUPID_RECORDERS selected=EventEnergyRecorder IncidentEnergyRecorder',
                         'backend=G4AnalysisManager schema=4',
                         'processed_events=3 rows_EventEnergy=0 rows_IncidentEnergy=0 file=none(empty)',
                         'CUPID_VERSIONS', 'ROOT=6.36.10', '/random/setSeeds 12567 76521',
                         '/cupid/geometry/lmoCount 2', 'CUPID_SOURCE /gun/position',
                         'CUPID_RUN_RESULT run=0 requested_events=32 processed_events=32 rows_EventEnergy=32 rows_IncidentEnergy=32',
                         'CUPID_RUN_RESULT run=1 requested_events=32 processed_events=32 rows_EventEnergy=0 rows_IncidentEnergy=0 file=none(empty)']:
            assert expected in log, f'{mode}: missing log content {expected}'
        for run, rows, incidents in [(0, 32, 32), (2, 1, 1)]:
            subprocess.run([str(checker), '--inspect', str(output / names[run]), str(rows), str(incidents)], check=True)
            validation = subprocess.run(
                [sys.executable, str(pathlib.Path(__file__).with_name('check_root_records.py')),
                 str(output / (names[run][:-5] + '_EventEnergy.root')), str(output / (names[run][:-5] + '_IncidentEnergy.root')), '--events', str(1 if run == 2 else 32), '--run', str(run)],
                capture_output=True, text=True, timeout=30)
            assert validation.returncode == 0, validation.stdout + validation.stderr
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()}
        again = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=30)
        assert again.returncode != 0, 'Existing job output was overwritten'
        assert before == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()}
        print('PASS', mode, 'merge, sparse rows, repeated runs, logs and overwrite protection', flush=True)
    # Repeated custom prefixes share one increasing suffix across both recorders.
    for mode, options in [('serial', ['--serial']), ('mt', ['--threads', '4'])]:
        duplicate = directory / f'duplicate-{mode}.mac'
        duplicate.write_text('/cupid/geometry/enable LMOArray\n/run/initialize\n'
                             '/gun/position 0 -350 0 mm\n/gun/particle e-\n/gun/energy 100 keV\n'
                             '/cupid/output/fileName repeated\n/run/beamOn 1\n/run/beamOn 1\n'
                             '/gun/particle geantino\n/run/beamOn 1\n/run/beamOn 1\n'
                             '/gun/particle e-\n/run/beamOn 1\n')
        output = directory / f'duplicate-{mode}'
        result = subprocess.run([str(executable), str(duplicate), *options, '--output', str(output)],
                                cwd=directory, capture_output=True, text=True, timeout=40)
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'Analysis_W' not in result.stdout + result.stderr
        expected_files = {'run.log'}
        # Empty runs 2 and 3 reserve their names even though their files are removed.
        log = (output / 'run.log').read_text()
        names = re.findall(r'CUPID_OUTPUT_NAME run=\d+ requested=repeated base=(\S+) clock=local', log)
        assert names == ['repeated.root'] + [f'repeated_{i}.root' for i in range(1, 5)], names
        for run, prefix in ((0, 'repeated'), (1, 'repeated_1'), (4, 'repeated_4')):
            expected_files.update(prefix + suffix for suffix in ('_EventEnergy.root', '_IncidentEnergy.root'))
            validation = subprocess.run([sys.executable, str(pathlib.Path(__file__).with_name('check_root_records.py')),
                str(output / (prefix + '_EventEnergy.root')), str(output / (prefix + '_IncidentEnergy.root')),
                '--events', '1', '--detectors', 'LMO0', '--run', str(run)], capture_output=True, text=True, timeout=30)
            assert validation.returncode == 0, validation.stdout + validation.stderr
        assert {p.name for p in output.iterdir()} == expected_files
    invalid_name = directory / 'invalid-name.mac'
    invalid_name.write_text('/run/initialize\n/cupid/output/fileName ../escape\n/run/beamOn 1\n')
    result = subprocess.run([str(executable), str(invalid_name), '--serial', '--output', str(directory / 'invalid-name')],
                            cwd=directory, capture_output=True, text=True, timeout=30)
    assert result.returncode != 0 and 'Output prefix must' in result.stdout + result.stderr
    # With no scoring volumes, both selected recorders must still finish cleanly.
    world_macro = directory / 'world.mac'
    world_macro.write_text('/run/initialize\n/gun/particle geantino\n/gun/energy 1 MeV\n/run/beamOn 1\n')
    world_output = directory / 'world'
    result = subprocess.run([str(executable), str(world_macro), '--threads', '4', '--output', str(world_output)],
                            cwd=directory, capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    assert {p.name for p in world_output.iterdir()} == {'run.log'}
    assert 'processed_events=1 rows_EventEnergy=0 rows_IncidentEnergy=0 file=none(empty)' in (world_output / 'run.log').read_text()
    bad_macro = directory / 'invalid.mac'
    bad_macro.write_text('/cupid/invalidCommand\n')
    failed_output = directory / 'failed'
    result = subprocess.run([str(executable), str(bad_macro), '--serial', '--output', str(failed_output)],
                            cwd=directory, capture_output=True, text=True, timeout=30)
    assert result.returncode != 0
    assert 'CUPID_ERROR: macro failed' in (failed_output / 'run.log').read_text()
print('All output integration checks passed.', flush=True)
