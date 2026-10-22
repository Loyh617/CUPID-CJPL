"""Scenario dispatch, source placement and error propagation using the real executable."""
import pathlib
import re
import subprocess
import sys

executable = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
passed = 0

def run(name, commands, mt=False, fail=False):
    global passed
    macro = output / (name + '.mac')
    macro.write_text('/random/setSeeds 12345 67890\n/run/verbose 1\n' + commands + '\n')
    args = ['--threads', '2'] if mt else ['--serial']
    result = subprocess.run([str(executable), str(macro), *args], cwd=output,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=90)
    log = result.stdout
    (output / (name + '.log')).write_text(log)
    if fail:
        assert result.returncode > 0, f'{name}: expected a controlled failure, got {result.returncode}'
    else:
        assert result.returncode == 0, f'{name}: exit {result.returncode}; see log'
        for error in ('CUPID_GENERATOR_ERROR', 'Run Aborted after', 'Event Must Be Aborted', 'GeomVol1002'):
            assert error not in log, f'{name}: {error}'
    passed += 1
    print('PASS', name, flush=True)
    return log

geometry = '/cupid/geometry/enable CrystalArray\n/cupid/geometry/crystalCount 2\n'
def source(isotope, mode, position):
    return (f'/cupid/generator/select calibration/{isotope}\n'
            f'/cupid/generator/position {mode}\n' + position + '\n/cupid/generator/verbose 1\n')

th = geometry + source('th232', 'point', '/cupid/generator/point -25 -350 0 mm')
log = run('th232_point', th + '/run/initialize\n/cupid/generator/check\n/tracking/verbose 1\n/run/beamOn 1')
assert 'source=calibration/th232 particle=Th232 x_mm=-25 y_mm=-350 z_mm=0 kinetic_MeV=0' in log
assert 'Particle = Pb208' in log

co = geometry + source('co56', 'volume', '/cupid/generator/volume Crystal1')
log = run('co56_volume_mt', co + '/run/initialize\n/cupid/generator/check\n/tracking/verbose 1\n/run/beamOn 24', mt=True)
rows = re.findall(r'CUPID_PRIMARY event=(\d+) source=calibration/co56 particle=Co56 x_mm=(\S+) y_mm=(\S+) z_mm=(\S+) kinetic_MeV=0', log)
assert sorted(int(row[0]) for row in rows) == list(range(24))
for _, x, y, z in rows:
    assert 2.5 < float(x) < 47.5 and -372.5 < float(y) < -327.5 and -22.5 < float(z) < 22.5
assert 'Particle = Fe56' in log

log = run('co56_repeated_runs', co + '/run/initialize\n/run/beamOn 2\n/run/beamOn 3')
assert log.count('CUPID_PRIMARY') == 5
assert re.search(r'Number of events processed : 2\b', log)
assert re.search(r'Number of events processed : 3\b', log)

missing = source('co56', 'volume', '/cupid/generator/volume MissingCrystal')
log = run('missing_volume_check', missing + '/run/initialize\n/cupid/generator/check', fail=True)
assert 'Source volume not found: MissingCrystal' in log
for mt in (False, True):
    log = run('missing_volume_run_' + str(mt), missing + '/run/initialize\n/run/beamOn 4', mt=mt, fail=True)
    assert 'CUPID_GENERATOR_ERROR: Source volume not found: MissingCrystal' in log

log = run('outside_world', source('co56', 'point', '/cupid/generator/point 3 0 0 m') +
          '/run/initialize\n/cupid/generator/check', fail=True)
assert 'Source point must be strictly inside World' in log
run('unimplemented_source', '/cupid/generator/select crystal_signal/0nubb', fail=True)
run('late_source_change', '/run/initialize\n/cupid/generator/select calibration/co56', fail=True)
run('unsupported_surface', '/cupid/generator/position surface', fail=True)
print(f'All {passed} generator checks passed.', flush=True)
