// test stub: no native HTTP in node tests
export const CapacitorHttp = {};
export const Capacitor = { getPlatform: () => 'web' };
export const registerPlugin = () => new Proxy({}, { get: () => async () => ({}) });
