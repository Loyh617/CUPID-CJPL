"""Check production Action merging, zero rows, idle workers, empty files, repeated runs and consolidated logs."""
import hashlib
import pathlib
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
/run/beamOn 1
/gun/particle geantino
/gun/position 0 1000 0 mm
/run/beamOn 3
""")
    for mode, options in [('serial', ['--serial']), ('mt', ['--threads', '4'])]:
        output = directory / mode
        command = [str(executable), str(macro), *options, '--output', str(output)]
        result = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
        assert {p.name for p in output.iterdir()} == {'run0_EventEnergy.root', 'run0_IncidentEnergy.root', 'run2_EventEnergy.root', 'run2_IncidentEnergy.root', 'run.log'}
        assert 'Analysis_W' not in result.stdout + result.stderr, result.stdout + result.stderr
        log = (output / 'run.log').read_text()
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
            subprocess.run([str(checker), '--inspect', str(output / f'run{run}.root'), str(rows), str(incidents)], check=True)
            validation = subprocess.run(
                [sys.executable, str(pathlib.Path(__file__).with_name('check_root_records.py')),
                 str(output / f'run{run}_EventEnergy.root'), str(output / f'run{run}_IncidentEnergy.root'), '--events', str(1 if run == 2 else 32), '--run', str(run)],
                capture_output=True, text=True, timeout=30)
            assert validation.returncode == 0, validation.stdout + validation.stderr
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()}
        again = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=30)
        assert again.returncode != 0, 'Existing job output was overwritten'
        assert before == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()}
        print('PASS', mode, 'merge, sparse rows, repeated runs, logs and overwrite protection', flush=True)
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
