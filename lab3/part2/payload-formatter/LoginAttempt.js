function decodeUplink(input) {
  var data = {};

  // Decode Timestamp (4 bytes, little-endian)
  var timestamp_raw =
    input.bytes[0] |
    (input.bytes[1] << 8) |
    (input.bytes[2] << 16) |
    (input.bytes[3] << 24);
  data.unix_time = timestamp_raw;
  // Convert Unix timestamp to a human-readable ISO string
  data.time_utc = new Date(timestamp_raw * 1000).toISOString();

  // Decode Success (1 byte)
  data.success_raw = input.bytes[4];
  data.success = data.success_raw === 1 ? "success" : "fail";

  // Decode Username (8 bytes)
  var username_bytes = input.bytes.slice(5, 13);
  // Convert byte array to string and trim any null characters
  data.username = String.fromCharCode
    .apply(null, username_bytes)
    .replace(/\0/g, "");

  return {
    data: data,
    warnings: [],
    errors: [],
  };
}
