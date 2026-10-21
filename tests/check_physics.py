"""Process registration and transport smoke checks, not physics validation."""
import pathlib
import re
import subprocess
import sys

executable = pathlib.Path(sys.argv[1]).resolve()
output = pathlib.Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)

def run(name, commands, patterns, threads=False):
    macro = output / (name + '.mac')
    macro.write_text('/random/setSeeds 12345 67890\n'
                     '/cupid/geometry/enable CrystalArray\n'
                     '/run/verbose 1\n'
                     '/run/initialize\n' + commands + '\n', encoding='utf-8')
    arguments = ['--threads', '2'] if threads else ['--serial']
    result = subprocess.run([str(executable), str(macro), *arguments], cwd=output,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=90)
    log = result.stdout
    (output / (name + '.log')).write_text(log, encoding='utf-8')
    assert result.returncode == 0, f'{name}: exit {result.returncode}; see log'
    assert 'CUPID_ENV' in log and 'physics=CUPIDPhysicsList-v1' in log
    assert 'GeomVol1002' not in log and 'GeomNav' not in log
    assert 'had005' not in log and 'Event Must Be Aborted' not in log
    assert 'Run Aborted after' not in log
    for pattern in patterns:
        assert re.search(pattern, log), f'{name}: missing {pattern!r}; see log'
    print('PASS', name, flush=True)

# Dump actual registered processes, then transport a muon and a neutron.
run('processes_and_transport', """
/particle/select gamma
/particle/process/dump
/particle/select e-
/particle/process/dump
/particle/select mu-
/particle/process/dump
/particle/select neutron
/particle/process/dump
/gun/position 0 -350 0 mm
/gun/direction 0 0 1
/gun/particle mu-
/gun/energy 1 GeV
/tracking/verbose 1
/run/beamOn 1
/gun/particle neutron
/gun/energy 1 MeV
/run/beamOn 1
""", [r'phot', r'compt', r'Rayl', r'eIoni', r'eBrem',
       r'muonNuclear', r'muMinusCaptureAtRest', r'neutronInelastic', r'nCapture',
       r'CrystalArray\.LMO.*muIoni', r'Particle = neutron',
       # Retired 2026-10-21: actions are placeholders; use native run summaries.
       # Restore only if CUPID_RUN_SUMMARY is reintroduced.
       # r'CUPID_RUN_SUMMARY run=1 events=1 aborted=0'])
       r'(?ms)^  Number of events processed : 1$.*^  Number of events processed : 1$'])

# A single Th-232 event must actually decay through daughter nuclei to Pb-208.
ion = """
/gun/particle ion
/gun/ion {z} {a} 0 0
/gun/energy 0 eV
/gun/position 0 -350 0 mm
/tracking/verbose 1
/run/beamOn {events}
"""
run('th232_chain', ion.format(z=90, a=232, events=1),
    [r'Particle = Th232', r'Particle = Ra228', r'Particle = Pb208',
     # Retired 2026-10-21: actions are placeholders; use native run summaries.
     # Restore only if CUPID_RUN_SUMMARY is reintroduced.
     # r'CUPID_RUN_SUMMARY run=0 events=1 aborted=0'])
     r'(?m)^  Number of events processed : 1$'])

# Exercise radioactive decays in worker threads with an independent isotope.
run('co56_mt', ion.format(z=27, a=56, events=4),
    [r'Particle = Co56', r'Particle = Fe56',
     # Retired 2026-10-21: actions are placeholders; use native run summaries.
     # Restore only if CUPID_RUN_SUMMARY is reintroduced.
     # r'CUPID_RUN_SUMMARY run=0 events=4 aborted=0'], threads=True)
     r'(?m)^  Number of events processed : 4$'], threads=True)

print('All 3 physics smoke checks passed.', flush=True)
