"""Check per-run directories/logs, native merging, path overrides and preserved results."""
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
reader = pathlib.Path(__file__).with_name('check_root_records.py')


def execute(macro, cwd, options, output=None):
    command = [str(executable), str(macro), *options]
    if output is not None:
        command += ['--output', str(output)]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=90)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'Analysis_W' not in result.stdout + result.stderr, result.stdout + result.stderr
    return result.stdout + result.stderr


def check_run(folder, run, events, rows, detectors='LMO0,LMO1'):
    prefix = folder.name
    log = (folder / 'run.log').read_text()
    # No event output or summaries from other runs may leak into this log.
    names = re.findall(r'CUPID_OUTPUT_NAME run=(\d+) requested=\S+ base=(\S+) clock=local', log)
    assert names == [(str(run), prefix + '.root')], names
    summaries = re.findall(r'CUPID_RUN_RESULT run=(\d+) ', log)
    assert summaries == [str(run)], summaries
    assert f'processed_events={events} rows_EventEnergy={rows} rows_IncidentEnergy={rows}' in log
    for token in ('CUPID_VERSIONS', 'ROOT=6.36.10', 'CUPID_COMMAND', 'CUPID_MACRO_BEGIN',
                  'CUPID_RANDOM_STATE_BEGIN', 'CUPID_SOURCE /gun/position'):
        assert token in log, (folder, token)
    expected = {'run.log'}
    if rows:
        expected.update(prefix + suffix for suffix in ('_EventEnergy.root', '_IncidentEnergy.root'))
        if detectors == 'LMO0,LMO1':
            subprocess.run([str(checker), '--inspect', str(folder / (prefix + '.root')), str(rows), str(rows)], check=True)
        result = subprocess.run([sys.executable, str(reader),
            str(folder / (prefix + '_EventEnergy.root')), str(folder / (prefix + '_IncidentEnergy.root')),
            '--events', str(events), '--run', str(run), '--detectors', *detectors.split(',')],
            capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stdout + result.stderr
    assert {p.name for p in folder.iterdir()} == expected


with tempfile.TemporaryDirectory(dir=base) as temporary:
    directory = pathlib.Path(temporary)
    (directory / 'geometry.mac').write_text('/cupid/geometry/enable LMOArray\n/cupid/geometry/lmoCount 2\n')
    for mode, options in [('serial', ['--serial']), ('mt', ['--threads', '4'])]:
        output = directory / (mode + '-cli')
        alternate = directory / (mode + '-macro')
        macro = directory / (mode + '.mac')
        macro.write_text(f"""/control/verbose 0
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
/cupid/output/directory {alternate.name}
/cupid/output/fileName calibration.root
/gun/particle e-
/gun/energy 100 keV
/gun/position 25 -350 0 mm
/run/beamOn 1
/cupid/output/fileName auto
/gun/particle geantino
/gun/position 0 1000 0 mm
/run/beamOn 3
""")
        stdout = execute(macro, directory, options, output)
        folders = [pathlib.Path(p.strip()) for p in re.findall(r'CUPID_RUN_DIRECTORY (.+)', stdout)]
        assert len(folders) == len(set(folders)) == 4, folders
        assert [p.parent for p in folders] == [output, output, alternate, alternate]
        for run in (0, 1, 3):
            assert re.fullmatch(r'run\d{8}_\d{6}(?:_[1-9]\d*)?', folders[run].name)
        assert folders[2].name == 'calibration'
        for run, events, rows in ((0, 32, 32), (1, 32, 0), (2, 1, 1), (3, 3, 0)):
            check_run(folders[run], run, events, rows)
            log = (folders[run] / 'run.log').read_text()
            assert '/random/setSeeds 12567 76521' in log and '/cupid/geometry/lmoCount 2' in log
        for parent in (output, alternate):
            assert len(list(parent.iterdir())) == 2
            assert all(p.is_dir() for p in parent.iterdir()), 'Loose output in parent directory'
        before = {p: hashlib.sha256(p.read_bytes()).hexdigest()
                  for parent in (output, alternate) for p in parent.rglob('*') if p.is_file()}
        # A later process may reuse the parent, but must leave every old run untouched.
        execute(macro, directory, options, output)
        assert all(hashlib.sha256(p.read_bytes()).hexdigest() == digest for p, digest in before.items())
        assert (alternate / 'calibration_1' / 'run.log').is_file()
        print('PASS', mode, 'per-run logs, directory overrides, ROOT readback and repeated jobs', flush=True)

        repeated = directory / f'repeated-{mode}.mac'
        repeated.write_text('/cupid/geometry/enable LMOArray\n/run/initialize\n'
                            '/gun/position 0 -350 0 mm\n/gun/particle e-\n/gun/energy 100 keV\n'
                            '/cupid/output/fileName repeated\n/run/beamOn 1\n/run/beamOn 1\n'
                            '/gun/particle geantino\n/run/beamOn 1\n/run/beamOn 1\n'
                            '/gun/particle e-\n/run/beamOn 1\n')
        output = directory / f'repeated-{mode}'
        execute(repeated, directory, options, output)
        assert {p.name for p in output.iterdir()} == {'repeated', *(f'repeated_{i}' for i in range(1, 5))}
        for run in range(5):
            folder = output / ('repeated' if run == 0 else f'repeated_{run}')
            check_run(folder, run, 1, 0 if run in (2, 3) else 1, 'LMO0')

    world = directory / 'world.mac'
    world.write_text('/run/initialize\n/cupid/output/fileName world\n/gun/particle geantino\n/gun/energy 1 MeV\n/run/beamOn 1\n')
    # The actual CLI default, without --output, is relative to the working directory.
    execute(world, directory, ['--threads', '4'])
    default = directory / 'results' / 'runs'
    assert {p.name for p in default.iterdir()} == {'world'}
    check_run(default / 'world', 0, 1, 0)

    # Directory settings also support absolute paths; invalid parents fail cleanly.
    absolute = directory / 'absolute.mac'
    absolute.write_text(world.read_text().replace('/cupid/output/fileName world',
        f'/cupid/output/directory "{directory / "absolute output"}"\n/cupid/output/fileName world'))
    execute(absolute, directory, ['--serial'], directory / 'unused-cli')
    assert not (directory / 'unused-cli').exists()
    check_run(directory / 'absolute output' / 'world', 0, 1, 0)
    invalid_parent = directory / 'not-a-directory'
    invalid_parent.write_text('preserve this file')
    result = subprocess.run([str(executable), str(world), '--serial', '--output', str(invalid_parent)],
                            cwd=directory, capture_output=True, text=True, timeout=30)
    assert result.returncode != 0
    assert invalid_parent.read_text() == 'preserve this file'

    # Without a begun run there is no run folder yet; errors remain visible on console.
    for name, commands in [('invalid-name', '/run/initialize\n/cupid/output/fileName ../escape\n/run/beamOn 1\n'),
                           ('invalid-macro', '/cupid/invalidCommand\n')]:
        macro = directory / (name + '.mac')
        macro.write_text(commands)
        output = directory / name
        result = subprocess.run([str(executable), str(macro), '--serial', '--output', str(output)],
                                cwd=directory, capture_output=True, text=True, timeout=30)
        assert result.returncode != 0
        assert not output.exists(), 'Failed pre-run setup created output artifacts'
        assert 'CUPID_ERROR' in result.stdout + result.stderr
print('All per-run output integration checks passed.', flush=True)
