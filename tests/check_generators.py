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

geometry = '/cupid/geometry/enable LMOArray\n/cupid/geometry/lmoCount 2\n'
gun = (geometry + '/cupid/generator/select gun\n/cupid/generator/verbose 1\n'
       '/cupid/generator/list\n/run/initialize\n/cupid/generator/check\n'
       '/gun/particle gamma\n/gun/energy 1 MeV\n/gun/position -25 -350 0 mm\n')
for mt in (False, True):
    log = run('gun_' + ('mt' if mt else 'serial'), gun + '/run/beamOn 4', mt=mt)
    assert 'CUPID_GENERATOR selected=gun configuration=/gun/' in log
    assert 'CUPID_GENERATOR_CHECK scope=selection result=PASS' in log
    rows = re.findall(r'CUPID_PRIMARY event=(\d+) source=gun particle=gamma x_mm=-25 y_mm=-350 z_mm=0 kinetic_MeV=1', log)
    assert sorted(map(int, rows)) == list(range(4))

log = run('gun_repeated_runs', gun + '/run/beamOn 2\n/run/beamOn 3')
assert log.count('CUPID_PRIMARY') == 5
assert re.search(r'Number of events processed : 2\b', log)
assert re.search(r'Number of events processed : 3\b', log)

for source in ('calibration/th232', 'calibration/co56', 'lmo_signal/2nubb', 'lmo_signal/0nubb'):
    log = run('unimplemented_' + source.replace('/', '_'),
              '/cupid/generator/select ' + source + '\n/run/initialize\n/run/beamOn 1', fail=True)
    assert 'CUPID_PRIMARY' not in log
for name, command in [('position', 'position point'), ('point', 'point 0 0 0 mm'), ('volume', 'volume LMO1')]:
    run('removed_' + name, '/cupid/generator/' + command, fail=True)
run('late_source_change', '/run/initialize\n/cupid/generator/select gun', fail=True)
print(f'All {passed} generator checks passed.', flush=True)
