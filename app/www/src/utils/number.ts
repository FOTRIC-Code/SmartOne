export const roundToMaxDecimals = (value: number, decimals = 2) => {
  if (!Number.isFinite(value)) return value;

  const factor = 10 ** decimals;
  return Math.round((value + Number.EPSILON) * factor) / factor;
};

export const normalizeDisplayNumber = (
  value: number | null | undefined,
  fallback: number,
  decimals = 2
) => roundToMaxDecimals(value ?? fallback, decimals);

export function roundFloatsDeep<T>(value: T): T {
  if (typeof value === 'number') {
    if (!Number.isFinite(value) || Number.isInteger(value)) return value;
    return roundToMaxDecimals(value, 2) as T;
  }
  if (Array.isArray(value)) {
    return value.map((item) => roundFloatsDeep(item)) as T;
  }
  if (value !== null && typeof value === 'object') {
    const out: Record<string, unknown> = {};
    for (const [k, v] of Object.entries(value as Record<string, unknown>)) {
      out[k] = roundFloatsDeep(v);
    }
    return out as T;
  }
  return value;
}
