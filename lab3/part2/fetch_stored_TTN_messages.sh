curl -sS -N -G "https://nam1.cloud.thethings.network/api/v3/as/applications/lab3-part2-40250986/packages/storage/uplink_message" \
  -H "Authorization: Bearer NNSXS.YI3PY7E2JPBS6HL2SZXBGDJP7GDCGG6EGYXOHKY.2IWDMVC7UDG625UKIB5KOPYORYLQGRCCMIIUIMP37YBLZB3CVQLA" \
  -H "Accept: text/event-stream" \
  -d "last=12h" \
| sed -u 's/^data: //' \
| jq -c 'try .result // . 
    | {timestamp: .received_at,
       username: .uplink_message.decoded_payload.username,
       success: .uplink_message.decoded_payload.success}
    | select(.username != null and .success != null)'