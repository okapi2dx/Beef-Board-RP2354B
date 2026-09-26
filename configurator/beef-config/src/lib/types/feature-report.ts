// WebHID returns the bytes supplied by the operating system. Depending on the
// platform, a feature report may include its report ID as the first byte.
export function featureReportPayload(
  report: DataView,
  reportId: number,
  expectedLengths?: readonly number[]
): DataView {
  if (expectedLengths?.includes(report.byteLength)) return report;

  const hasId = report.byteLength > 0 && report.getUint8(0) === reportId;
  if (hasId && (!expectedLengths || expectedLengths.includes(report.byteLength - 1))) {
    return new DataView(report.buffer, report.byteOffset + 1, report.byteLength - 1);
  }

  if (expectedLengths) throw new Error(`Invalid feature report ${reportId} length`);
  return report;
}
