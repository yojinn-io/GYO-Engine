"""Product probe evidence: streaming integrity of the action evidence records."""
import json
from pathlib import Path


def records(path, *, legacy=False):
    count, ended = 0, False
    with Path(path).open() as stream:
        for number, line in enumerate(stream, 1):
            try:
                value = json.loads(line)
                if ended:
                    raise ValueError('record after evidence_end')
                if value.get('kind') == 'evidence_end':
                    if value['records'] != count or value['lost'] != 0:
                        raise ValueError('record count mismatch or diagnostic loss')
                    ended = True
                else:
                    count += 1
                    yield value
            except (ValueError, KeyError, TypeError) as error:
                raise ValueError(f'{path}:{number}: corrupt action evidence: {error}') from error
    if not ended and not legacy:
        raise ValueError(f'{path}: missing evidence_end')


def frames(output):
    path = output/'action-frames.jsonl'
    if path.exists():
        yield from records(path)
    else:
        yield from json.loads((output/'action-frames.json').read_text())
