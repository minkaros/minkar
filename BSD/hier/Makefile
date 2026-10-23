# Base file system hierarchy (architecture plan, Phase 2): hier.spec,
# created in the sysroot before the rest of the build installs into it.

SUBDIR =

all: hier

# A real /etc, /var or /tmp left by an older build is merged into /private
# and replaced by its link.
hier: ${.CURDIR}/hier.spec
	@set -e; root="${SYSROOT_DIR}"; mkdir -p "$$root"; \
	grep -v '^[[:space:]]*\(#\|$$\)' ${.CURDIR}/hier.spec | \
	while read type arg path; do \
	    case $$type in \
	    d)  mkdir -p "$$root/$$path"; \
	        chown 0:0 "$$root/$$path"; chmod "$$arg" "$$root/$$path" ;; \
	    l)  if [ -d "$$root/$$path" ] && [ ! -L "$$root/$$path" ]; then \
	            echo "hier: moving $$path into $$arg"; \
	            cp -a "$$root/$$path/." "$$root/$$arg/"; \
	            rm -rf "$$root/$$path"; \
	        fi; \
	        ln -sfn "$$arg" "$$root/$$path" ;; \
	    *)  echo "hier.spec: bad entry: $$type $$arg $$path" >&2; exit 1 ;; \
	    esac; \
	done

.PHONY: all hier

.include <bsd.prog.mk>
