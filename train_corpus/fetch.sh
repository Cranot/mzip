#!/usr/bin/env bash
# Fetch real-world corpus for dict training (NOT overlapping with real_bench/).
# Each category targets ~200 KB of diverse, real content.
set -e
cd "$(dirname "$0")"

fetch() {
  local cat="$1" url="$2" out="$3"
  if [ -f "$cat/$out" ] && [ -s "$cat/$out" ]; then return; fi
  curl -sSf "$url" -o "$cat/$out" 2>/dev/null || rm -f "$cat/$out"
}

# Markdown / text — diverse real-world README, CHANGELOG, docs.
fetch md "https://raw.githubusercontent.com/django/django/main/README.rst" django_readme.rst
fetch md "https://raw.githubusercontent.com/python/cpython/main/README.rst" cpython_readme.rst
fetch md "https://raw.githubusercontent.com/torvalds/linux/master/README" linux_readme
fetch md "https://raw.githubusercontent.com/postgres/postgres/master/README.md" postgres_readme.md
fetch md "https://raw.githubusercontent.com/redis/redis/unstable/README.md" redis_readme.md
fetch md "https://raw.githubusercontent.com/nginx/nginx/master/README" nginx_readme
fetch md "https://raw.githubusercontent.com/sveltejs/svelte/main/CHANGELOG.md" svelte_changelog.md
fetch md "https://raw.githubusercontent.com/vitejs/vite/main/CHANGELOG.md" vite_changelog.md
fetch md "https://raw.githubusercontent.com/babel/babel/main/CHANGELOG.md" babel_changelog.md
fetch md "https://raw.githubusercontent.com/expressjs/express/master/Changelog.md" express_changelog.md
fetch md "https://raw.githubusercontent.com/torvalds/linux/master/Documentation/process/index.rst" linux_proc.rst
fetch md "https://raw.githubusercontent.com/nodejs/node/main/CHANGELOG.md" node_changelog.md
fetch md "https://raw.githubusercontent.com/git/git/master/README.md" git_readme.md
fetch md "https://raw.githubusercontent.com/curl/curl/master/CHANGES.md" curl_changes.md

# YAML / k8s manifests
fetch yaml "https://raw.githubusercontent.com/kubernetes/examples/master/guestbook/all-in-one/guestbook-all-in-one.yaml" guestbook.yaml
fetch yaml "https://raw.githubusercontent.com/kubernetes/examples/master/staging/storage/redis/redis-master-deployment.yaml" redis-master.yaml
fetch yaml "https://raw.githubusercontent.com/kubernetes/examples/master/staging/cloud-controller-manager/sample-cloud-controller-manager.yaml" ccm.yaml
fetch yaml "https://raw.githubusercontent.com/kubernetes/examples/master/cassandra/cassandra-statefulset.yaml" cassandra.yaml
fetch yaml "https://raw.githubusercontent.com/prometheus/prometheus/main/documentation/examples/prometheus.yml" prom.yml
fetch yaml "https://raw.githubusercontent.com/grafana/grafana/main/docker-compose.yaml" grafana-dc.yaml
fetch yaml "https://raw.githubusercontent.com/elastic/helm-charts/main/elasticsearch/values.yaml" es-values.yaml
fetch yaml "https://raw.githubusercontent.com/jaegertracing/jaeger-kubernetes/main/all-in-one/jaeger-all-in-one-template.yml" jaeger.yml
fetch yaml "https://raw.githubusercontent.com/istio/istio/master/manifests/profiles/default.yaml" istio.yaml
fetch yaml "https://raw.githubusercontent.com/argoproj/argo-cd/master/manifests/install.yaml" argocd.yaml
fetch yaml "https://raw.githubusercontent.com/open-telemetry/opentelemetry-collector/main/examples/k8s/otel-config.yaml" otel.yaml
fetch yaml "https://raw.githubusercontent.com/cert-manager/cert-manager/master/deploy/charts/cert-manager/values.yaml" certmgr.yaml
fetch yaml "https://raw.githubusercontent.com/traefik/traefik-helm-chart/master/traefik/values.yaml" traefik.yaml

# Terraform / HCL
fetch hcl "https://raw.githubusercontent.com/terraform-aws-modules/terraform-aws-vpc/master/main.tf" aws_vpc_main.tf
fetch hcl "https://raw.githubusercontent.com/terraform-aws-modules/terraform-aws-vpc/master/variables.tf" aws_vpc_vars.tf
fetch hcl "https://raw.githubusercontent.com/terraform-aws-modules/terraform-aws-eks/master/main.tf" aws_eks_main.tf
fetch hcl "https://raw.githubusercontent.com/terraform-aws-modules/terraform-aws-eks/master/variables.tf" aws_eks_vars.tf
fetch hcl "https://raw.githubusercontent.com/terraform-aws-modules/terraform-aws-rds/master/main.tf" aws_rds_main.tf
fetch hcl "https://raw.githubusercontent.com/terraform-google-modules/terraform-google-network/master/main.tf" gcp_net_main.tf
fetch hcl "https://raw.githubusercontent.com/Azure/terraform-azurerm-vnet/main/main.tf" azure_vnet.tf
fetch hcl "https://raw.githubusercontent.com/hashicorp/terraform-provider-aws/main/website/docs/r/instance.html.markdown" aws_inst_doc.md
fetch hcl "https://raw.githubusercontent.com/hashicorp/learn-terraform-deploy-eks-cluster/main/main.tf" learn_eks.tf
fetch hcl "https://raw.githubusercontent.com/hashicorp/learn-terraform-deploy-eks-cluster/main/variables.tf" learn_eks_vars.tf
fetch hcl "https://raw.githubusercontent.com/hashicorp/learn-terraform-aws-instance/main/main.tf" learn_inst.tf
fetch hcl "https://raw.githubusercontent.com/cloudposse/terraform-aws-ec2-instance/main/main.tf" cp_ec2.tf

# SQL schemas / DDL
fetch sql "https://raw.githubusercontent.com/lerocha/chinook-database/master/ChinookDatabase/DataSources/Chinook_PostgreSql.sql" chinook_pg.sql
fetch sql "https://raw.githubusercontent.com/lerocha/chinook-database/master/ChinookDatabase/DataSources/Chinook_MySql.sql" chinook_mysql.sql
fetch sql "https://raw.githubusercontent.com/datacharmer/test_db/master/employees.sql" employees.sql
fetch sql "https://raw.githubusercontent.com/datacharmer/test_db/master/load_employees.dump" employees_dump.sql
fetch sql "https://raw.githubusercontent.com/datacharmer/test_db/master/load_departments.dump" departments_dump.sql
fetch sql "https://raw.githubusercontent.com/sakila-sample/sakila-mysql-schema/main/sakila-schema.sql" sakila_schema.sql

# XML
fetch xml "https://raw.githubusercontent.com/spring-projects/spring-boot/main/pom.xml" spring_pom.xml
fetch xml "https://raw.githubusercontent.com/apache/maven/master/pom.xml" maven_pom.xml
fetch xml "https://raw.githubusercontent.com/apache/kafka/trunk/pom.xml" kafka_pom.xml
fetch xml "https://raw.githubusercontent.com/apache/tomcat/main/build.xml" tomcat_build.xml
fetch xml "https://raw.githubusercontent.com/apache/zookeeper/master/pom.xml" zk_pom.xml
fetch xml "https://raw.githubusercontent.com/apache/lucene/main/lucene/build.xml" lucene_build.xml
fetch xml "https://raw.githubusercontent.com/jenkinsci/jenkins/master/pom.xml" jenkins_pom.xml
fetch xml "https://raw.githubusercontent.com/elastic/elasticsearch/main/build.xml" es_build.xml
fetch xml "https://raw.githubusercontent.com/spring-projects/spring-framework/main/build.gradle" spring_build.gradle

# Code (mixed: Python, Go, JS, Rust, Java)
fetch code "https://raw.githubusercontent.com/django/django/main/django/views/generic/base.py" django_views.py
fetch code "https://raw.githubusercontent.com/python/cpython/main/Lib/json/encoder.py" cpython_json.py
fetch code "https://raw.githubusercontent.com/golang/go/master/src/net/http/server.go" go_http_server.go
fetch code "https://raw.githubusercontent.com/golang/go/master/src/encoding/json/decode.go" go_json_decode.go
fetch code "https://raw.githubusercontent.com/expressjs/express/master/lib/router/index.js" express_router.js
fetch code "https://raw.githubusercontent.com/nodejs/node/main/lib/http.js" node_http.js
fetch code "https://raw.githubusercontent.com/rust-lang/rust/master/library/core/src/option.rs" rust_option.rs
fetch code "https://raw.githubusercontent.com/rust-lang/rust/master/library/std/src/io/mod.rs" rust_io.rs
fetch code "https://raw.githubusercontent.com/openjdk/jdk/master/src/java.base/share/classes/java/util/HashMap.java" jdk_hashmap.java
fetch code "https://raw.githubusercontent.com/openjdk/jdk/master/src/java.base/share/classes/java/util/ArrayList.java" jdk_arraylist.java

echo "done"
for d in md yaml hcl sql xml code; do
  count=$(ls "$d" 2>/dev/null | wc -l)
  size=$(du -sb "$d" 2>/dev/null | awk '{print $1}')
  echo "  $d: $count files, $size bytes"
done
