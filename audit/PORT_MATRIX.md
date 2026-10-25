# Original-Quellbestand: Portierungsmatrix

Die Dateien des Originalprojekts wurden **nicht verändert**. Diese Matrix erfasst alle Dateien; die Spalte `Status` ist keine Zusage funktionaler Vollständigkeit.

| Bereich | Dateien | Python-Symbole |
|---|---:|---:|
| AI, simulation, cognition, database | 37 | 210 |
| Enterprise web platform | 29 | 0 |
| Native C/OpenCL, data, docs | 14 | 0 |
| Original regression tests | 76 | 215 |
| Other | 24 | 0 |
| Security / maintenance tools | 12 | 27 |
| Web gateway / legacy interface | 33 | 282 |

## Dateispezifische Übersicht

| Datei | Status |
|---|---|
| `www/logout.php` | NOT_PORTED |
| `www/webauthn.php` | NOT_PORTED |
| `www/telemetry_api.php` | NOT_PORTED |
| `www/e2ee.php` | NOT_PORTED |
| `www/blogs.php` | NOT_PORTED |
| `www/fun.php` | NOT_PORTED |
| `www/my_blog.php` | NOT_PORTED |
| `www/plugins.php` | NOT_PORTED |
| `www/ingest_manifest.php` | NOT_PORTED |
| `www/api.php` | NOT_PORTED |
| `www/site_texts.php` | NOT_PORTED |
| `www/admin.php` | NOT_PORTED |
| `www/dashboard.php` | NOT_PORTED |
| `www/messages.php` | NOT_PORTED |
| `www/forum.php` | NOT_PORTED |
| `www/webauthn_manifest.php` | NOT_PORTED |
| `www/bootstrap.php` | NOT_PORTED |
| `www/index.php` | NOT_PORTED |
| `www/download_my_data.php` | NOT_PORTED |
| `www/blog.php` | NOT_PORTED |
| `www/thread.php` | NOT_PORTED |
| `www/profile.php` | NOT_PORTED |
| `www/privacy.php` | NOT_PORTED |
| `tests/test_enterprise_v1201_hardening.py` | NOT_PORTED |
| `tests/test_strict_vram_certify_transport.py` | NOT_PORTED |
| `tests/test_web_action_token_integrity.py` | NOT_PORTED |
| `tests/test_vram_residency_audit.py` | NOT_PORTED |
| `tests/test_client_markdown_vault_v12125.py` | NOT_PORTED |
| `tests/test_direct_ingest_token_discipline_hotfix.py` | NOT_PORTED |
| `tests/test_long_markdown_content_v12124.py` | NOT_PORTED |
| `tests/test_fun_plugins_ui_integration_v12120.py` | NOT_PORTED |
| `tests/test_enterprise_v120_extensions.py` | NOT_PORTED |
| `tests/test_direct_gpu_ingest.py` | NOT_PORTED |
| `tests/test_zero_logic_session_binding.py` | NOT_PORTED |
| `tests/test_privacy_rights.py` | NOT_PORTED |
| `tests/test_native_gpu_residency_contract.py` | NOT_PORTED |
| `tests/test_native_strict_certification_gate_v18e.py` | NOT_PORTED |
| `tests/test_enterprise_content.py` | NOT_PORTED |
| `tests/test_memory_probe_classification.py` | NOT_PORTED |
| `tests/test_session_token_drift.py` | NOT_PORTED |
| `tests/test_snapshot_residency.py` | NOT_PORTED |
| `tests/test_native_dll_path_resolution.py` | NOT_PORTED |
| `tests/test_enterprise_evolution_v1217.py` | NOT_PORTED |
| `tests/test_autosnapshot_persistence.py` | NOT_PORTED |
| `tests/test_admin_vram_evidence_bundle.py` | NOT_PORTED |
| `tests/test_native_snapshot_runtime_v18d.py` | NOT_PORTED |
| `tests/test_gpu_crypto_reporting.py` | NOT_PORTED |
| `tests/test_shutdown_semantics.py` | NOT_PORTED |
| `tests/test_driver_path_resolution.py` | NOT_PORTED |
| `tests/test_admin_overview_payload_hotfix.py` | NOT_PORTED |
| `tests/test_php_safe_rendering.py` | NOT_PORTED |
| `tests/test_snapshot_path_fallback.py` | NOT_PORTED |
| `tests/test_markdown_rendering_v12123.py` | NOT_PORTED |
| `tests/test_plugin_attractor_system.py` | NOT_PORTED |
| `tests/test_native_startup_paths.py` | NOT_PORTED |
| `tests/test_enterprise_plugins_v1217.py` | NOT_PORTED |
| `tests/test_media_attractor_system.py` | NOT_PORTED |
| `tests/test_scheduled_heartbeat_audit.py` | NOT_PORTED |
| `tests/test_zero_logic_gateway_allowlist.py` | NOT_PORTED |
| `tests/test_residency_external_probe.py` | NOT_PORTED |
| `tests/test_admin_cms_rights.py` | NOT_PORTED |
| `Mycelia_Database-main/SymBio_Interface.h` | NOT_PORTED |
| `Mycelia_Database-main/CipherCore_NoiseCtrl.c` | NOT_PORTED |
| `Mycelia_Database-main/CC_OpenCL.c` | NOT_PORTED |
| `Mycelia_Database-main/CipherCore_NoiseCtrl.h` | NOT_PORTED |
| `html/mycelia_web_mysql.py` | NOT_PORTED |
| `html/mycelia_platform.py` | NOT_PORTED |
| `html/api.php` | NOT_PORTED |
| `html/setup_db.php` | NOT_PORTED |
| `html/mycelia_web_server.py` | NOT_PORTED |
| `html/index.php` | NOT_PORTED |
| `html/mycelia_chat_engine.py` | NOT_PORTED |
| `html/db_proxy.py` | NOT_PORTED |
| `html/profile.php` | NOT_PORTED |
| `html/mycelia_web_shop.py` | NOT_PORTED |
| `tools/mycelia_federation_sync.py` | NOT_PORTED |
| `tools/mycelia_strict_vram_certify.py` | NOT_PORTED |
| `tools/diagnose_native_gpu_envelope.py` | NOT_PORTED |
| `tools/mycelia_memory_probe.py` | NOT_PORTED |
| `tools/generate_native_hash_manifest.py` | NOT_PORTED |
| `tools/generate_localhost_tls.py` | NOT_PORTED |
| `tools/mycelia_heartbeat_audit.py` | NOT_PORTED |
| `html/native/mycelia_gpu_envelope_contract.c` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/query_sql.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/__init__.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/streamlit_app.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/main.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/import_sql.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/io/__init__.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/io/sql_importer.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/cognition/cognitive_core.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/cognition/observer_net.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/cognition/__init__.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/cognition/dynamic_database.py` | PARTIAL_CPP_PORT: signature, noise, pattern lifecycle, associative ranking and native associative_query |
| `Mycelia_Database-main/mycelia_ai/visualization/renderer.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/visualization/__init__.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/simulation/quantum_oracle.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/simulation/__init__.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/simulation/mycelia_world.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/core/__init__.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/core/gpu_tensor.py` | NOT_PORTED |
| `Mycelia_Database-main/mycelia_ai/core/driver.py` | NOT_PORTED |
| `www/assets/client-markdown-vault.js` | NOT_PORTED |
| `www/assets/markdown-copy.js` | NOT_PORTED |
| `www/assets/webauthn.js` | NOT_PORTED |
| `www/assets/direct-ingest.js` | NOT_PORTED |
| `www/assets/e2ee.js` | NOT_PORTED |


## Delta v0.2

| Original-Symbol | C++20 | Test | Status |
|---|---|---|---|
| `ObserverNetwork.encode` | `ObserverNetwork::encode` | `mycelia_observer_tests` | Kernparität (Mittelwerte, Fallback, tension) |
| `DynamicAssociativeDatabase.observe` | `DynamicAssociativeDatabase::observe` | Goldenvektor aus Original-Python | Kernparität für numerische Felder |
| `DynamicAssociativeDatabase._hash_series` | Teil von `observe` | SHA256-Goldenvektor | Testvektor paritätisch |
| `_rank_patterns` | `rank_patterns` | C++ Ranking-Test | Reihenfolge/Similarity, Randfälle offen |
| `memory_snapshot` | `memory_snapshot` | C++ Basistest | Teilport: native Pattern statt Dict |
| `_project_signature` | PORTED_CPP | Python-Golden-Vektor bestanden | Python-MT19937 und getrandbits-Randint |

Vollständige Portierung/Windows-Gedächtnis **nicht abgeschlossen**. Die vorherigen Auditdaten bleiben erhalten.


### Version 0.4 – SQL-Dump-Reader

| Ursprungsmodul | C++20-Port | Status |
|---|---|---|
| `mycelia_ai/io/sql_importer.py`: `CREATE TABLE` / `INSERT INTO` / Typkonvertierung / `fetch_rows` / `list_tables` | `src/sql_dump.hpp/.cpp` | Kern-Subset portiert, Golden-Referenz und Test; kein vollständiger SQL-Parser |
| `mycelia_ai/cognition/dynamic_database.py`: `store_sql_record` / `query_sql_like` / `associative_sql_lookup` | – | OFFEN: DAD-Anbindung nicht simuliert |
| `mycelia_ai/cognition/cognitive_core.py`: `import_sql_table` | – | OFFEN: Reihen zu Attraktoren überführen |

Release-Test: 4/4 CTest bestanden, Assertions in allen C++-Testquelldateien explizit auch im Release-Build aktiviert. Keine Python-Runtime erforderlich. Kein Windows-Build behauptet.
