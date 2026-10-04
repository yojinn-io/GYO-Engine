"""Explicit bounded v5 action/gameplay matrix; no GUI, soak or old-v4 execution."""
import argparse
import json
from pathlib import Path
from types import SimpleNamespace
from action_probe import run_case
from acceptance_util import PROTOCOL_VERSION


def matrix():
    cases=[dict(name='clean-'+str(fps),mode='baseline',fps=fps,milliseconds=0,fault_at=1.5) for fps in (60,30,144)]
    cases += [dict(name=mode,mode=mode,fps=60,milliseconds=0,fault_at=1.5) for mode in
              ('network0','network20','network40','burst2','loss-shot','loss-result','loss-ack','duplicate-reorder-conflict','drain-stall')]
    # Each axis is an explicit short case. Do not multiply all FPS/network axes.
    for mode in ('upstream','downstream','socket-path','gateway','host-ipc'):
        for ms in (250,1000):
            cases.append(dict(name=f'{mode}-{ms}ms',mode=mode,fps=60,milliseconds=ms,
                              fault_at=4.05 if mode in ('gateway','host-ipc') else 1.5))
    # Downstream alone preserves authority's lethal sequence while its results
    # cross death/respawn. Normal recovery budget still applies to alive A.
    for phase,at in (('death',6.85),('respawn',9.6)):
        cases.append(dict(name=f'downstream-{phase}-1000ms',mode='downstream',fps=60,milliseconds=1000,fault_at=at))
    cases.append(dict(name='cross-life',mode='cross-life',fps=60,milliseconds=0,fault_at=5.75,
                      expected_override={'dead-shot':'StaleLife after first life request ID3 is held until life2'},
                      result_hold_seconds=4.5))
    return cases


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('match','gateway','probe','arena','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--case',choices=[c['name'] for c in matrix()])
    parser.add_argument('--list-only',action='store_true')
    args=parser.parse_args()
    cases=[c for c in matrix() if not args.case or c['name']==args.case]
    if args.list_only: print(json.dumps(cases,indent=2));return 0
    for name in ('match','gateway','probe','arena'):
        setattr(args,name,getattr(args,name).resolve())
        if not getattr(args,name).is_file():parser.error('Missing '+name)
    args.output=args.output.resolve();args.output.mkdir(parents=True,exist_ok=False)
    (args.output/'planned-matrix.json').write_text(json.dumps({'protocol':PROTOCOL_VERSION,'cases':cases,'seconds_per_probe':16,
        'clean_denominator':'all predeclared legal action edges, separate intentional rejections',
        'recovery_seconds':1.5,'stable_movement_seconds':.25,'long_run':False},indent=2)+'\n')
    results=[]
    for case in cases:
        opts=SimpleNamespace(**vars(args),gameplay_v5=True,case_name=case['name'],fps=case['fps'],fault_at=case['fault_at'])
        result=run_case(opts,case['mode'],case['milliseconds']);result['case']=case;results.append(result)
        print(json.dumps({'case':case['name'],'passed':result['passed'],'errors':result['errors']}),flush=True)
        summary={'passed':len(results)==len(cases) and all(r['passed'] for r in results),'requested_cases':len(cases),
                 'completed_cases':len(results),'cases':results,'protocol':PROTOCOL_VERSION,'long_run':False}
        (args.output/'gameplay-matrix.json').write_text(json.dumps(summary,indent=2)+'\n')
        if not result['passed']:break
    return 0 if summary['passed'] else 1


if __name__=='__main__':raise SystemExit(main())
