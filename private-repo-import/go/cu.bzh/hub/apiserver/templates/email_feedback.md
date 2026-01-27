---
description: Feedback
sender-name: noreply@cu.bzh
sender-email: noreply@cu.bzh
title: "💬 Cubzh feedback"
---

FROM: {{ .Username }} (#{{ .Usernumber }})
{{ if .Email }}EMAIL: {{ .Email }}{{ end }}

{{ .Message }}
