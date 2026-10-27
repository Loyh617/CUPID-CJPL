"""Check production Action merging, zero rows, repeated runs and consolidated logs."""
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
""")
    for mode, options in [('serial', ['--serial']), ('mt', ['--threads', '4'])]:
        output = directory / mode
        command = [str(executable), str(macro), *options, '--output', str(output)]
        result = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=90)
        assert result.returncode == 0, result.stdout + result.stderr
        assert {p.name for p in output.iterdir()} == {'run0.root', 'run1.root', 'run.log'}
        log = (output / 'run.log').read_text()
        for expected in ['CUPID_VERSIONS', 'ROOT=6.36.10', '/random/setSeeds 12567 76521',
                         '/cupid/geometry/lmoCount 2', 'CUPID_SOURCE /gun/position',
                         'CUPID_RUN_RESULT run=0 requested_events=32 processed_events=32 event_rows=32 incident_rows=32',
                         'CUPID_RUN_RESULT run=1 requested_events=32 processed_events=32 event_rows=0 incident_rows=32']:
            assert expected in log, f'{mode}: missing log content {expected}'
        for run, rows in [(0, 32), (1, 0)]:
            subprocess.run([str(checker), '--inspect', str(output / f'run{run}.root'), str(rows), '32'], check=True)
        before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()}
        again = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=30)
        assert again.returncode != 0, 'Existing job output was overwritten'
        assert before == {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir()}
        print('PASS', mode, 'merge, sparse rows, repeated runs, logs and overwrite protection', flush=True)
    bad_macro = directory / 'invalid.mac'
    bad_macro.write_text('/cupid/invalidCommand\n')
    failed_output = directory / 'failed'
    result = subprocess.run([str(executable), str(bad_macro), '--serial', '--output', str(failed_output)],
                            cwd=directory, capture_output=True, text=True, timeout=30)
    assert result.returncode != 0
    assert 'CUPID_ERROR: macro failed' in (failed_output / 'run.log').read_text()
print('All output integration checks passed.', flush=True)
