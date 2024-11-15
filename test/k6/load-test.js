import http from 'k6/http';
import exec from 'k6/execution';
import { check, sleep } from 'k6';

export const options = {
  vus: 10,
  duration: '30s',
};

export default function() {

  const baseUrl = __ENV.url || 'http://localhost:8888';

  const td = http.get(baseUrl);
  check(td, {'get thing description status was 200': (r) => r.status == 200 });

  const ps = http.get(baseUrl + '/properties');
  check(ps, {'get properties status was 200': (r) => r.status == 200 });

  const as = http.get(baseUrl + '/actions');
  check(as, {'get actions status was 200': (r) => r.status == 200 });

  const es = http.get(baseUrl + '/events');
  check(es, {'get events status was 200': (r) => r.status == 200 });

  const headers = {
    headers: {
        'Content-Type': 'application/json',
    },
  };

  const on = exec.scenario.iterationInTest % 2 == 0;
  const onProp = http.put(baseUrl + '/properties/on', JSON.stringify({"on":on}), headers);
  check(onProp, {'put on property status was 200': (r) => r.status == 200 });

  const brightness = 10 * (exec.scenario.iterationInTest % 10);
  const duration = 1 + 200 * (exec.scenario.iterationInTest % 5);
  const fadePayload = {"fade":{"input":{"brightness":brightness, "duration":duration}}};
  const fade = http.post(baseUrl + '/actions', JSON.stringify(fadePayload), headers);
  check(fade, {'post fade actions status was 201': (r) => r.status == 201 });
}
